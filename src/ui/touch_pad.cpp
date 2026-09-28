#include "touch_pad.h"

#include <algorithm>
#include <filesystem>
#include <utility>

#include "app/settings.h"
#include "platform/platform.h"

namespace retro3do {
namespace {
struct Action { const char* id; retro3do_pad_button button; };
constexpr Action actions[] = {
    {"dir:up", RETRO3DO_PAD_UP}, {"dir:down", RETRO3DO_PAD_DOWN},
    {"dir:left", RETRO3DO_PAD_LEFT}, {"dir:right", RETRO3DO_PAD_RIGHT},
    {"a", RETRO3DO_PAD_A}, {"b", RETRO3DO_PAD_B}, {"c", RETRO3DO_PAD_C},
    {"play", RETRO3DO_PAD_PLAY}, {"stop", RETRO3DO_PAD_STOP},
    {"lshift", RETRO3DO_PAD_LEFT_SHIFT}, {"rshift", RETRO3DO_PAD_RIGHT_SHIFT},
};

bool is_imported_legacy_layout(const touchpad::Layout& layout) {
    if (layout.profile != "3do" || layout.extras.size() != std::size(actions)) return false;
    for (const auto& cluster : layout.clusters)
        if (cluster.second.visible) return false;
    for (const auto& action : actions) {
        const auto count = std::count_if(layout.extras.begin(), layout.extras.end(),
            [&](const touchpad::Extra& extra) {
                if (extra.id != action.id) return false;
                // These labels only came from the old SDL pad. Shared-pad
                // extras use PLAY/STOP, so a user-designed layout is distinct.
                if (extra.id == "play") return extra.label == "P";
                if (extra.id == "stop") return extra.label == "X";
                return true;
            });
        if (count != 1) return false;
    }
    return true;
}
}

TouchPad::TouchPad(Press press) : press_(std::move(press)) {
    visible_ = RETRO3DO_MOBILE;
    sink_.action = [this](const std::string& id, bool down) {
        if (!press_) return;
        for (const auto& action : actions)
            if (id == action.id) { press_(action.button, down); return; }
    };
    sink_.directions = [this](bool up, bool down, bool left, bool right) {
        const bool next[] = {up, down, left, right};
        for (int i = 0; i < 4; ++i) {
            if (next[i] == directions_[i]) continue;
            directions_[i] = next[i];
            if (press_) press_(actions[i].button, next[i]);
        }
    };
    reset_layout(width_, height_);
}

void TouchPad::release_all() { overlay_.release_all(sink_); }

void TouchPad::set_visible(bool visible) {
    if (!visible) { release_all(); set_editing(false); }
    visible_ = visible;
}

void TouchPad::set_physical_gamepad_present(bool present) {
    if (present) { release_all(); set_editing(false); }
    gamepad_present_ = present;
}

void TouchPad::set_active(bool active) {
    if (!active && active_) release_all();
    active_ = active;
}

void TouchPad::set_editing(bool editing) {
    if (editing == overlay_.editing()) return;
    release_all();
    overlay_.set_editing(editing);
}

void TouchPad::reset_layout(int width, int height) {
    width_ = std::max(1, width); height_ = std::max(1, height);
    area_pos_ = ImVec2(0, 0);
    area_size_ = ImVec2(float(width_), float(height_));
    release_all();
    const auto& profile = touchpad::profile_3do();
    auto layout = touchpad::Layout::defaults(profile);
    const float scale = std::clamp(std::min(width_, height_) / 720.0f, 0.5f, 3.0f);
    for (auto& cluster : layout.clusters) cluster.second.scale = scale;
    overlay_.set(&profile, layout);
}

void TouchPad::load_layout(const Settings& settings) {
    layout_dir_ = std::filesystem::path(settings.path()).parent_path().string();
    if (layout_dir_.empty()) layout_dir_ = ".";
    const auto& profile = touchpad::profile_3do();
    release_all();
    const std::filesystem::path path = touchpad::Layout::file_for(layout_dir_, profile);
    reset_layout(width_, height_);
    if (std::filesystem::exists(path)) {
        auto layout = touchpad::Layout::load(layout_dir_, profile);
        if (is_imported_legacy_layout(layout)) {
            // The first shared-pad build hid every group and recreated all
            // eleven old controls as extras. In the editor both sets appeared.
            // Only repair that signature; genuinely edited shared pads survive.
            std::error_code error;
            std::filesystem::copy_file(path, path.string() + ".legacy",
                                      std::filesystem::copy_options::skip_existing, error);
            if (error) SDL_Log("Could not back up the old touch layout: %s", error.message().c_str());
            if (!overlay_.layout().save(layout_dir_))
                SDL_Log("Could not save the replacement 3DO controller layout");
        } else {
            overlay_.set(&profile, layout);
        }
    }
    set_visible(settings.get_bool(settings_key::kTouchControls, visible_));
}

void TouchPad::save_layout(Settings& settings) const {
    settings.set_bool(settings_key::kTouchControls, visible_);
    if (!layout_dir_.empty() && !overlay_.layout().save(layout_dir_))
        SDL_Log("Could not save the touch controller layout in %s", layout_dir_.c_str());
}

bool TouchPad::handle_event(const SDL_Event& event, int width, int height,
                            bool pointer_available) {
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST ||
        event.type == SDL_EVENT_WILL_ENTER_BACKGROUND) {
        release_all();
        return false;
    }
    if ((!visible() && !editing()) || !active_ || width <= 0 || height <= 0)
        return false;
    // A release must still reach an existing touch when a menu captures the
    // pointer. Starting a gesture over a widget must never press the pad.
    if (!pointer_available && event.type != SDL_EVENT_FINGER_UP &&
        event.type != SDL_EVENT_FINGER_CANCELED && event.type != SDL_EVENT_MOUSE_BUTTON_UP)
        return false;
    return overlay_.handle(event, ImVec2(float(width), float(height)),
                            area_pos_, area_size_, sink_);
}

void TouchPad::draw(const ImVec2& pos, const ImVec2& size) {
    area_pos_ = pos; area_size_ = size;
    if (size.x <= 0 || size.y <= 0 || (!visible() && !editing())) return;
    overlay_.draw(ImGui::GetBackgroundDrawList(), pos, size);
}

bool TouchPad::draw_designer(bool close_popup) {
    bool changed = false;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                  viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (overlay_.has_selection() && !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        !ImGui::IsPopupOpen("Pad settings"))
        ImGui::OpenPopup("Pad settings");
    if (ImGui::BeginPopupModal("Pad settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        changed |= overlay_.designer_controls(&sink_);
        if (overlay_.has_selection()) {
            ImGui::Separator();
            changed |= overlay_.cluster_panel();
        } else if (ImGui::Button("Done")) {
            ImGui::CloseCurrentPopup();
        }
        if (close_popup || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            overlay_.clear_selection();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    const bool dragged = overlay_.take_dirty();
    return changed || dragged;
}
}  // namespace retro3do
