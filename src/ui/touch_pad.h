// Retro-3DO adapter for the retro_touch_pad controller used by Saturn and DOS.
#pragma once

#include <functional>
#include "retro3do.h"
#include "ui/retro_touch_pad.h"

namespace retro3do {
class Settings;

class TouchPad {
public:
    using Press = std::function<void(retro3do_pad_button, bool)>;
    explicit TouchPad(Press press = {});
    TouchPad(const TouchPad&) = delete;
    TouchPad& operator=(const TouchPad&) = delete;

    void set_visible(bool visible);
    bool enabled() const { return visible_; }
    bool visible() const { return visible_ && !gamepad_present_; }
    bool physical_gamepad_present() const { return gamepad_present_; }
    void set_physical_gamepad_present(bool present);
    void set_active(bool active);
    void release_all();

    void load_layout(const Settings& settings);
    void save_layout(Settings& settings) const;
    void reset_layout(int window_width, int window_height);
    void set_editing(bool editing);
    bool editing() const { return overlay_.editing(); }
    bool take_dirty() { return overlay_.take_dirty(); }

    // All coordinates use SDL window points, matching ImGui. Framebuffer
    // density is applied only by the renderer, never by the input path.
    bool handle_event(const SDL_Event& event, int window_width, int window_height,
                      bool pointer_available = true);
    void draw(const ImVec2& pos, const ImVec2& size);
    bool draw_designer(bool close_popup = false);
    const touchpad::Layout& layout() const { return overlay_.layout(); }

private:
    touchpad::Overlay overlay_;
    touchpad::Sink sink_;
    Press press_;
    bool visible_ = false;
    bool gamepad_present_ = false;
    bool active_ = false;
    bool directions_[4] = {};
    int width_ = 1280, height_ = 720;
    ImVec2 area_pos_{0, 0}, area_size_{1280, 720};
    std::string layout_dir_;
};
}  // namespace retro3do
