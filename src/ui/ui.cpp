#include "ui.h"

#include "core/guide.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "core/console.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "platform/android_storage.h"
#include "platform/platform.h"

namespace retro3do {
namespace {

const ImVec4 kCyan(0.18f, 0.84f, 0.88f, 1.0f);
const ImVec4 kGreen(0.36f, 0.86f, 0.58f, 1.0f);
const ImVec4 kAmber(1.00f, 0.68f, 0.28f, 1.0f);
const ImVec4 kRed(1.00f, 0.37f, 0.38f, 1.0f);
const ImVec4 kViolet(0.70f, 0.45f, 1.00f, 1.0f);
const ImVec4 kPink(1.00f, 0.38f, 0.68f, 1.0f);
const ImVec4 kBlue(0.30f, 0.58f, 1.00f, 1.0f);
const ImVec4 kMuted(0.55f, 0.60f, 0.68f, 1.0f);

ImU32 colour(const ImVec4& value) {
    return ImGui::ColorConvertFloat4ToU32(value);
}

void page_heading(const char* title, const char* subtitle) {
    ImGui::TextColored(kCyan, "%s", title);
    if (ImGui::GetContentRegionAvail().x > ImGui::CalcTextSize(subtitle).x +
                                             ImGui::GetStyle().ItemSpacing.x)
        ImGui::SameLine();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(kMuted, "%s", subtitle);
    ImGui::PopTextWrapPos();
    ImGui::Separator();
    ImGui::Spacing();
}

void section_heading(const char* title, const char* detail) {
    ImGui::TextColored(kCyan, "%s", title);
    if (detail != nullptr && detail[0] != '\0') {
        ImGui::TextColored(kMuted, "%s", detail);
    }
    ImGui::Spacing();
}

ImVec4 tinted(const ImVec4& colour_value, float strength, float alpha = 1.0f);

void same_line_if_fits(const char* label) {
    const float width = ImGui::CalcTextSize(label, nullptr, true).x +
        std::max(ImGui::GetFrameHeight(), ImGui::GetStyle().FramePadding.x * 2.0f) +
        ImGui::GetStyle().ItemInnerSpacing.x;
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width <= right)
        ImGui::SameLine();
}

void begin_panel(const char* id, float width, bool stacked) {
    const ImGuiChildFlags flags = ImGuiChildFlags_Borders |
        (stacked ? ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize : 0);
    ImGui::BeginChild(id, ImVec2(width, 0.0f), flags, ImGuiWindowFlags_NoScrollbar);
    ImGui::PushTextWrapPos(0.0f);
}

void end_panel() {
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
}

void alphabet_filter(char& selected, const ImVec4& accent, float scale) {
    constexpr char groups[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ#";
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(3.0f * scale, 5.0f * scale));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f * scale, 8.0f * scale));
    const float available = ImGui::GetContentRegionAvail().x;
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const int max_columns = std::max(1, static_cast<int>((available + gap) /
                                                       (26.0f * scale + gap)));
    const int rows = (27 + max_columns - 1) / max_columns;
    const int columns = (27 + rows - 1) / rows;
    const float width = (available - gap * (columns - 1)) / columns;
    for (int i = 0; i < 27; ++i) {
        if (i % columns != 0) ImGui::SameLine();
        const char group = groups[i];
        char label[2] = {group, '\0'};
        if (selected == group)
            ImGui::PushStyleColor(ImGuiCol_Button, tinted(accent, 0.38f));
        const bool pressed = ImGui::Button(group == '#' ? "0-9" : label,
                                           ImVec2(width, 0.0f));
        if (selected == group) ImGui::PopStyleColor();
        if (pressed) selected = selected == group ? 0 : group;
    }
    ImGui::PopStyleVar(2);
    ImGui::Spacing();
}

bool wide_button(const char* label, float height) {
    return ImGui::Button(label, ImVec2(-1.0f, height));
}

char card_initial(const std::string& name) {
    for (unsigned char c : name) {
        if (std::isalnum(c)) return static_cast<char>(std::toupper(c));
    }
    return '?';
}

ImVec4 card_accent(const std::string& name) {
    static const ImVec4 palette[] = {kCyan, kViolet, kPink, kAmber, kGreen,
                                     kBlue};
    unsigned hash = 2166136261u;
    for (unsigned char c : name) hash = (hash ^ c) * 16777619u;
    return palette[hash % (sizeof(palette) / sizeof(palette[0]))];
}

ImVec4 tinted(const ImVec4& colour_value, float strength, float alpha) {
    return ImVec4(0.035f + colour_value.x * strength,
                  0.045f + colour_value.y * strength,
                  0.070f + colour_value.z * strength, alpha);
}

void draw_card_title(ImDrawList* draw, const std::string& title,
                     const ImVec2& position, float max_width, ImU32 colour_value,
                     float font_size) {
    // Keep titles to one draw command. Thousands of tiny per-glyph geometry
    // submissions made the SDL renderer drop later commands on dense library
    // screens, which showed up as alternating cards with no title.
    ImFont* font = ImGui::GetFont();
    std::string shown = title;
    const char* suffix = "...";
    const float width_limit = std::max(1.0f, max_width);
    if (font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, shown.c_str()).x >
        width_limit) {
        while (!shown.empty()) {
            shown.pop_back();
            const std::string candidate = shown + suffix;
            if (font->CalcTextSizeA(font_size, FLT_MAX, 0.0f,
                                    candidate.c_str()).x <= width_limit) {
                shown = candidate;
                break;
            }
        }
    }
    const float shown_width =
        font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, shown.c_str()).x;
    draw->AddText(font, font_size,
                  ImVec2(position.x + std::max(0.0f,
                                               (width_limit - shown_width) * 0.5f),
                         position.y),
                  colour_value, shown.c_str());
}

}  // namespace

Ui::Ui() = default;

Ui::~Ui() {
    shutdown();
}

bool Ui::init(SDL_Window* window, SDL_Renderer* renderer) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
#if RETRO3DO_MOBILE
    // iPad and iPhone want this for the same reason Android does: it widens
    // ImGui's hit areas to finger size. Not on macOS, which has a pointer.
    io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
#endif
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildRounding = 8.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 6.0f;
    style.WindowPadding = ImVec2(18.0f, 18.0f);
    style.FramePadding = ImVec2(12.0f, 8.0f);
    style.ItemSpacing = ImVec2(10.0f, 10.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.035f, 0.045f, 0.070f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.055f, 0.070f, 0.105f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.14f, 0.19f, 0.26f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.075f, 0.095f, 0.140f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.11f, 0.16f, 0.22f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.09f, 0.13f, 0.19f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.12f, 0.35f, 0.40f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.12f, 0.55f, 0.60f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.28f, 0.34f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.12f, 0.38f, 0.43f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = kCyan;
    style.Colors[ImGuiCol_SliderGrab] = kCyan;
    style.Colors[ImGuiCol_Text] = ImVec4(0.91f, 0.94f, 0.97f, 1.0f);
    style.Colors[ImGuiCol_TextDisabled] = kMuted;

    // Bridge logical points to framebuffer pixels on Retina displays.
    const float density = SDL_GetWindowPixelDensity(window);
    if (density > 0.0f) SDL_SetRenderScale(renderer, density, density);
    window_ = window;
    base_style_ = std::make_unique<ImGuiStyle>(style);
    update_scale();

    if (!ImGui_ImplSDL3_InitForSDLRenderer(window, renderer)) return false;
    if (!ImGui_ImplSDLRenderer3_Init(renderer)) return false;

    renderer_ = renderer;
    load_splash_texture();

    splash_started_ms_ = SDL_GetTicks();
    last_pointer_activity_ms_ = splash_started_ms_;
    initialised_ = true;
    return true;
}

void Ui::wake_menu_button() {
    last_pointer_activity_ms_ = SDL_GetTicks();
}

void Ui::hide_launcher() {
    show_launcher_ = false;
    session_available_ = true;
    wake_menu_button();
}

void Ui::set_remembered_bios(const std::string& name,
                             const std::string& target) {
    std::snprintf(bios_path_buffer_, sizeof(bios_path_buffer_), "%s",
                  name.empty() ? target.c_str() : name.c_str());
    bios_target_ = target;
}

void Ui::set_remembered_disc(const std::string& name,
                             const std::string& target) {
    std::snprintf(disc_path_buffer_, sizeof(disc_path_buffer_), "%s",
                  name.empty() ? target.c_str() : name.c_str());
    disc_target_ = target;
}

void Ui::set_game_library(const std::vector<LibraryGame>& games) {
    library_.replace(games);
}

void Ui::configure_video(int renderer_backend, bool widescreen, bool bezel,
                         bool crt_effect, const std::string& gpu_driver_name) {
    renderer_backend_ = renderer_backend == 1 ? 1 : 0;
    widescreen_ = widescreen;
    bezel_ = bezel;
    crt_effect_ = crt_effect;
    gpu_driver_name_ = gpu_driver_name;
}

void Ui::set_renderer_status(const std::string& actual,
                             const std::string& startup_message) {
    actual_renderer_ = actual;
    renderer_startup_message_ = startup_message;
}

void Ui::set_gpu_driver_status(const std::string& name,
                               const std::string& message) {
    gpu_driver_name_ = name;
    gpu_driver_message_ = message;
}

void Ui::load_splash_texture() {
    if (renderer_ == nullptr || splash_texture_ != nullptr) return;
    SDL_Surface* surface = SDL_LoadBMP("retro3do-splash.bmp");
    if (surface == nullptr) {
        surface = SDL_LoadBMP("assets/branding/retro3do-splash.bmp");
    }
    if (surface == nullptr) return;
    splash_texture_ = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_DestroySurface(surface);
    if (splash_texture_ != nullptr) {
        SDL_SetTextureScaleMode(splash_texture_, SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(splash_texture_, SDL_BLENDMODE_BLEND);
    }
}

void Ui::configure_setup(bool complete, const std::string& bios_folder_name,
                         const std::string& bios_folder_target,
                         const std::string& games_folder_name,
                         const std::string& games_folder_target) {
    setup_complete_ = complete;
    bios_folder_name_ = bios_folder_name;
    bios_folder_target_ = bios_folder_target;
    games_folder_name_ = games_folder_name;
    games_folder_target_ = games_folder_target;
    if (!complete) wizard_step_ = WizardStep::Welcome;
}

void Ui::shutdown() {
    if (!initialised_) return;
    if (splash_texture_ != nullptr) {
        SDL_DestroyTexture(splash_texture_);
        splash_texture_ = nullptr;
    }
    release_ui_texture();
    if (renderer_ != nullptr) ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    renderer_ = nullptr;
    initialised_ = false;
}

void Ui::suspend_renderer() {
    if (!initialised_ || renderer_ == nullptr) return;
    if (splash_texture_ != nullptr) {
        SDL_DestroyTexture(splash_texture_);
        splash_texture_ = nullptr;
    }
    release_ui_texture();
    ImGui_ImplSDLRenderer3_Shutdown();
    renderer_ = nullptr;
}

bool Ui::resume_renderer(SDL_Renderer* renderer) {
    if (!initialised_ || renderer == nullptr || renderer_ != nullptr) {
        return false;
    }
    if (!ImGui_ImplSDLRenderer3_Init(renderer)) return false;
    renderer_ = renderer;
    const float density = SDL_GetWindowPixelDensity(window_);
    if (density > 0.0f) SDL_SetRenderScale(renderer, density, density);
    load_splash_texture();
    return true;
}

void Ui::update_scale() {
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window_, &width, &height);
    const float fit = std::min(std::max(1, width) / 800.0f,
                               std::max(1, height) / 480.0f);
    const float limit = std::max(1.0f, std::min(4.0f, width / 360.0f));
    // Display scale includes pixel density; renderer scaling already handles it.
    const float density = std::max(1.0f, SDL_GetWindowPixelDensity(window_));
    const float logical_display_scale = SDL_GetWindowDisplayScale(window_) / density;
    const float scale = std::clamp(std::max(fit, logical_display_scale),
                                    1.0f, limit);
    if (scale == scale_) return;
    scale_ = scale;
    ImGui::GetStyle() = *base_style_;
    ImGui::GetStyle().ScaleAllSizes(scale_);
    ImGui::GetIO().FontGlobalScale = scale_;
}

void Ui::process_event(const SDL_Event& event) {
    if (!initialised_) return;
    ImGui_ImplSDL3_ProcessEvent(&event);
    if (event.type == SDL_EVENT_FINGER_DOWN ||
        event.type == SDL_EVENT_FINGER_MOTION ||
        event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        // A touch anywhere wakes the unobtrusive in-game Menu control.  This
        // runs before the on-screen pad consumes the event, so pressing a game
        // button also counts as activity without stealing that press.
        wake_menu_button();
    }
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
        (event.key.scancode == SDL_SCANCODE_ESCAPE ||
         event.key.scancode == SDL_SCANCODE_AC_BACK)) {
        menu_requested_ = true;
    }
}

// Use the panel under the initial touch for the entire gesture. The SDL
// backend labels touch-generated mouse events, so taps still use the normal
// widgets while a vertical drag cancels their pending click before release.
void Ui::update_touch_scroll() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.MouseSource != ImGuiMouseSource_TouchScreen) {
        scroll_window_ = 0;
        scroll_dragging_ = false;
        return;
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImGuiWindow* window = GImGui->HoveredWindow;
        while (window && (window->ScrollMax.y <= 0.0f ||
                         (window->Flags & ImGuiWindowFlags_NoScrollWithMouse))) {
            window = (window->Flags & ImGuiWindowFlags_ChildWindow)
                         ? window->ParentWindow : nullptr;
        }
        scroll_window_ = window ? window->ID : 0;
        scroll_start_y_ = io.MousePos.y;
        scroll_start_offset_ = window ? window->Scroll.y : 0.0f;
        scroll_dragging_ = false;
    }
    ImGuiWindow* window = ImGui::FindWindowByID(scroll_window_);
    if (window && window->WasActive) {
        const float distance = scroll_start_y_ - io.MousePos.y;
        if (io.MouseDown[0] && std::abs(distance) > 8.0f * scale_)
            scroll_dragging_ = true;
        if (scroll_dragging_) {
            ImGui::ClearActiveID();
            ImGui::SetScrollY(window, std::clamp(scroll_start_offset_ + distance,
                                               0.0f, window->ScrollMax.y));
        }
    }
    if (!io.MouseDown[0]) {
        scroll_window_ = 0;
        scroll_dragging_ = false;
    }
}

bool Ui::wants_mouse() const {
    return initialised_ && ImGui::GetIO().WantCaptureMouse;
}

bool Ui::wants_keyboard() const {
    return initialised_ && ImGui::GetIO().WantCaptureKeyboard;
}

// A notched or islanded handheld reserves screen edges the system draws over.
// In landscape the cutout sits on a long edge, directly over the navigation
// rail, so without this the first characters of the page are hidden behind it.
//
// The launcher already positions everything from the viewport's work area, so
// narrowing the work area to the safe area moves the whole UI at once and
// leaves the game picture underneath drawn full-bleed, which is what it should
// be. On a device with no insets SDL reports the whole window and this is a
// no-op.
void Ui::apply_safe_area() {
    if (window_ == nullptr) return;

    SDL_Rect safe{};
    if (!SDL_GetWindowSafeArea(window_, &safe)) return;
    if (safe.w <= 0 || safe.h <= 0) return;

    int window_w = 0;
    int window_h = 0;
    SDL_GetWindowSize(window_, &window_w, &window_h);
    if (window_w <= 0 || window_h <= 0) return;

    // SDL reports the safe area in window coordinates and ImGui's viewport is
    // in the same space, so the insets carry across directly.
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr) return;

    const float left = static_cast<float>(safe.x);
    const float top = static_cast<float>(safe.y);
    const float right = static_cast<float>(window_w - (safe.x + safe.w));
    const float bottom = static_cast<float>(window_h - (safe.y + safe.h));

    viewport->WorkPos = ImVec2(viewport->Pos.x + left, viewport->Pos.y + top);
    viewport->WorkSize = ImVec2(viewport->Size.x - left - right,
                                viewport->Size.y - top - bottom);
}


UiIntent Ui::build(Console& console, bool emulating, double display_fps,
                   double emulated_fps, double frame_ms, u64 underruns,
                   bool touch_visible, bool touch_editing) {
    UiIntent intent;
    if (!initialised_) return intent;

    update_scale();
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    apply_safe_area();
    update_touch_scroll();


    if (!splash_complete_) {
        draw_splash();
        if (SDL_GetTicks() - splash_started_ms_ >= 1400u ||
            ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            splash_complete_ = true;
        }
    } else if (!setup_complete_) {
        menu_requested_ = false;
        poll_setup_picker(intent);
        draw_setup_wizard(console, intent);
    } else if (show_launcher_) {
        menu_requested_ = false;
        draw_launcher(console, emulating, touch_visible, touch_editing, intent);
    } else {
        if (menu_requested_) {
            if (quick_menu_) {
                quick_menu_ = false;
                if (resume_after_menu_ && !emulating) intent.toggle_pause = true;
            } else {
                open_quick_menu(emulating, intent);
            }
            menu_requested_ = false;
        }
        draw_overlay(console, emulating, display_fps, emulated_fps, frame_ms,
                     underruns, touch_visible, touch_editing, intent);
        if (quick_menu_) {
            draw_quick_menu(console, emulating, touch_visible, touch_editing,
                            intent);
        }
    }

    // Last, so it sits over everything, and only once the splash has gone --
    // a walkthrough appearing under a splash screen would be read by nobody.
    if (splash_complete_ && !guide_seen_ && setup_complete_) {
        guide_seen_ = true;
        open_guide();
    }
    draw_guide(intent);

    ImGui::Render();
    return intent;
}

void Ui::draw_splash() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::Begin("##splash", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 top = viewport->Pos;
    const ImVec2 bottom(top.x + viewport->Size.x, top.y + viewport->Size.y);
    draw->AddRectFilledMultiColor(top, bottom, IM_COL32(6, 11, 24, 255),
                                  IM_COL32(8, 37, 52, 255),
                                  IM_COL32(22, 8, 42, 255),
                                  IM_COL32(6, 11, 24, 255));

    const ImVec2 centre(top.x + viewport->Size.x * 0.5f,
                        top.y + viewport->Size.y * 0.45f);
    const float icon_size =
        std::min(300.0f * scale_, viewport->Size.y * 0.70f);
    if (splash_texture_ != nullptr) {
        draw->AddImage(
            ImTextureRef(static_cast<ImTextureID>(
                reinterpret_cast<intptr_t>(splash_texture_))),
            ImVec2(centre.x - icon_size * 0.5f,
                   centre.y - icon_size * 0.5f),
            ImVec2(centre.x + icon_size * 0.5f,
                   centre.y + icon_size * 0.5f));
    } else {
        ImGui::SetWindowFontScale(2.55f);
        const char* title = "RETRO-3DO";
        const ImVec2 title_size = ImGui::CalcTextSize(title);
        ImGui::SetCursorScreenPos(ImVec2(centre.x - title_size.x * 0.5f,
                                        centre.y - title_size.y * 0.5f));
        ImGui::TextUnformatted(title);
        ImGui::SetWindowFontScale(1.0f);
    }

    const char* strap = "THE INTERACTIVE MULTIPLAYER, REIMAGINED";
    const ImVec2 strap_size = ImGui::CalcTextSize(strap);
    ImGui::SetCursorScreenPos(ImVec2(centre.x - strap_size.x * 0.5f,
                                    centre.y + icon_size * 0.5f +
                                        10.0f * scale_));
    ImGui::TextColored(kCyan, "%s", strap);
    ImGui::End();
}

void Ui::poll_setup_picker(UiIntent& intent) {
    if (wizard_pick_ == WizardPick::None) return;
    const DocumentEntry picked = AndroidStorage::consume_picked_folder();
    if (picked.uri.empty()) return;

    intent.folder_path = picked.uri;
    intent.folder_name = picked.name;
    if (wizard_pick_ == WizardPick::Bios) {
        bios_folder_name_ = picked.name;
        bios_folder_target_ = picked.uri;
        intent.bios_folder_chosen = true;
    } else {
        games_folder_name_ = picked.name;
        games_folder_target_ = picked.uri;
        intent.games_folder_chosen = true;
    }
    wizard_pick_ = WizardPick::None;
}

void Ui::draw_setup_wizard(Console& console, UiIntent& intent) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("##setup_backdrop", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilledMultiColor(
        viewport->WorkPos,
        ImVec2(viewport->WorkPos.x + viewport->WorkSize.x,
               viewport->WorkPos.y + viewport->WorkSize.y),
        IM_COL32(6, 12, 25, 255), IM_COL32(7, 30, 43, 255),
        IM_COL32(20, 8, 36, 255), IM_COL32(6, 12, 25, 255));

    const float width = std::min(750.0f * scale_, viewport->WorkSize.x * 0.90f);
    const float height = std::min(430.0f * scale_, viewport->WorkSize.y * 0.90f);
    ImGui::SetCursorPos(ImVec2((viewport->WorkSize.x - width) * 0.5f,
                              (viewport->WorkSize.y - height) * 0.5f));
    ImGui::BeginChild("##setup_card", ImVec2(width, height),
                      ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(kCyan, "RETRO-3DO SETUP");
    const int step = wizard_step_ == WizardStep::Welcome ? 1
                   : wizard_step_ == WizardStep::Bios    ? 2
                   : wizard_step_ == WizardStep::Games   ? 3
                                                          : 4;
    ImGui::SameLine();
    ImGui::TextColored(kMuted, "STEP %d OF 4", step);
    ImGui::Separator();
    ImGui::Spacing();

    switch (wizard_step_) {
        case WizardStep::Welcome:
            ImGui::SetWindowFontScale(1.55f);
            ImGui::TextUnformatted("WELCOME TO RETRO-3DO");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::TextWrapped(
                "Setup needs access to two folders: one containing your legally "
                "obtained 3DO BIOS and one containing your games. Android will "
                "show its folder picker for each choice.");
            ImGui::Spacing();
            ImGui::TextColored(kGreen, "SCOPED STORAGE");
            ImGui::TextWrapped(
                "Retro-3DO only receives access to the folders you choose. It "
                "does not request permission to read the rest of your device.");
            ImGui::Spacing();
            if (wide_button("BEGIN SETUP", 44.0f * scale_)) {
                wizard_step_ = WizardStep::Bios;
            }
            ImGui::Spacing();
            if (wide_button("TRY BUILT-IN HARDWARE DEMO", 44.0f * scale_)) {
                // This bypasses setup for the current run only. No setting is
                // saved, so the legal user-BIOS setup returns next launch.
                setup_complete_ = true;
                show_launcher_ = false;
                session_available_ = true;
                intent.start_demo = true;
                wake_menu_button();
            }
            break;

        case WizardStep::Bios:
            ImGui::SetWindowFontScale(1.45f);
            ImGui::TextUnformatted("CHOOSE YOUR BIOS FOLDER");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::TextWrapped(
                "Select the folder containing your 3DO .rom or .bin BIOS. "
                "Retro-3DO will find and validate it automatically.");
            ImGui::Spacing();
            ImGui::TextColored(bios_folder_target_.empty() ? kMuted : kCyan,
                               "%s", bios_folder_target_.empty()
                                         ? "No folder selected"
                                         : bios_folder_name_.c_str());
            if (wide_button(bios_folder_target_.empty() ? "SELECT BIOS FOLDER"
                                                         : "CHANGE BIOS FOLDER",
                            42.0f * scale_)) {
                wizard_pick_ = WizardPick::Bios;
                AndroidStorage::pick_folder();
            }
            if (wizard_pick_ == WizardPick::Bios) {
                ImGui::TextColored(kAmber, "Waiting for Android folder selection...");
            } else if (console.bios_loaded()) {
                ImGui::TextColored(kGreen, "BIOS found and loaded successfully");
            } else if (!bios_folder_target_.empty()) {
                ImGui::TextColored(kRed,
                                   "No valid BIOS was found. Choose another folder.");
            }
            ImGui::BeginDisabled(!console.bios_loaded());
            if (wide_button("CONTINUE", 42.0f * scale_)) {
                wizard_step_ = WizardStep::Games;
            }
            ImGui::EndDisabled();
            break;

        case WizardStep::Games:
            ImGui::SetWindowFontScale(1.45f);
            ImGui::TextUnformatted("CHOOSE YOUR GAMES FOLDER");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::TextWrapped(
                "Select the top-level folder containing your ISO, BIN/CUE, IMG "
                "or CHD games. Subfolders are scanned automatically.");
            ImGui::Spacing();
            ImGui::TextColored(games_folder_target_.empty() ? kMuted : kCyan,
                               "%s", games_folder_target_.empty()
                                         ? "No folder selected"
                                         : games_folder_name_.c_str());
            if (wide_button(games_folder_target_.empty() ? "SELECT GAMES FOLDER"
                                                          : "CHANGE GAMES FOLDER",
                            42.0f * scale_)) {
                wizard_pick_ = WizardPick::Games;
                AndroidStorage::pick_folder();
            }
            if (wizard_pick_ == WizardPick::Games) {
                ImGui::TextColored(kAmber, "Waiting for Android folder selection...");
            } else if (!games_folder_target_.empty()) {
                ImGui::TextColored(kGreen, "%zu game%s currently in the library",
                                   library_.games().size(),
                                   library_.games().size() == 1 ? "" : "s");
            }
            ImGui::BeginDisabled(games_folder_target_.empty());
            if (wide_button("CONTINUE", 42.0f * scale_)) {
                wizard_step_ = WizardStep::Finish;
            }
            ImGui::EndDisabled();
            break;

        case WizardStep::Finish:
            ImGui::SetWindowFontScale(1.55f);
            ImGui::TextColored(kGreen, "SETUP COMPLETE");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::TextWrapped(
                "Your BIOS is ready and %zu game%s %s available. You can change "
                "either folder later from the System page.",
                library_.games().size(), library_.games().size() == 1 ? "" : "s",
                library_.games().size() == 1 ? "is" : "are");
            ImGui::Spacing();
            if (wide_button("OPEN GAME LIBRARY", 46.0f * scale_)) {
                setup_complete_ = true;
                show_launcher_ = true;
                page_ = Page::Library;
                intent.setup_finished = true;
            }
            break;
    }

    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    ImGui::End();
}

void Ui::draw_launcher(Console& console, bool emulating, bool touch_visible,
                       bool touch_editing, UiIntent& intent) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("Retro-3DO", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

    const ImVec2 nav_top = ImGui::GetCursorScreenPos();
    const float total_width = ImGui::GetContentRegionAvail().x;
    // Apple's guidance is that an iOS app is never quit by the app itself, and
    // a visible quit control is a known review rejection. The system gesture is
    // the way out of a handheld; on a desktop the button still is. It has to
    // come out of the count as well as the layout or the grid keeps its slot.
    const int action_count = 5 + (RETRO3DO_PLATFORM_IOS ? 0 : 1) +
                             (session_available_ ? 1 : 0);
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const int nav_columns = std::max(1, std::min(action_count,
        static_cast<int>((total_width + gap) / (84.0f * scale_ + gap))));
    const int nav_rows = (action_count + nav_columns - 1) / nav_columns;
    const float action_width = (total_width - gap * (nav_columns - 1)) / nav_columns;
    const float nav_height = nav_rows * (40.0f * scale_ + ImGui::GetStyle().ItemSpacing.y);
    int nav_index = 0;
    auto next_nav = [&] {
        if (nav_index++ % nav_columns != 0) ImGui::SameLine();
    };

    auto nav_button = [&](const char* label, Page page) {
        next_nav();
        const bool selected = page_ == page;
        if (selected) {
            ImVec4 accent = kAmber;
            if (page == Page::Library) accent = kCyan;
            if (page == Page::Settings) accent = kViolet;
            if (page == Page::About) accent = kGreen;
            ImGui::PushStyleColor(ImGuiCol_Button, tinted(accent, 0.38f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  tinted(accent, 0.52f));
        }
        if (ImGui::Button(label, ImVec2(action_width, 40.0f * scale_))) page_ = page;
        if (selected) ImGui::PopStyleColor(2);
    };
    nav_button("LIBRARY", Page::Library);
    next_nav();
    // No DEMO button here on purpose. The demonstration is part of being shown
    // round the app, so the walkthrough is the only thing that offers it, and
    // leaving it returns there rather than dropping the user on the library.
    nav_button("SETTINGS", Page::Settings);
    nav_button("SYSTEM", Page::System);
    if (ImGui::Button("GUIDE", ImVec2(action_width, 40.0f * scale_))) open_guide();
    nav_button("ABOUT", Page::About);
    if (session_available_) {
        next_nav();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.34f, 0.27f, 1.0f));
        if (ImGui::Button("RESUME", ImVec2(action_width, 40.0f * scale_))) {
            show_launcher_ = false;
            wake_menu_button();
            if (!emulating) intent.toggle_pause = true;
        }
        ImGui::PopStyleColor();
    }
#if !RETRO3DO_PLATFORM_IOS
    next_nav();
    if (ImGui::Button("EXIT", ImVec2(action_width, 40.0f * scale_))) {
        intent.quit = true;
    }
#endif
    ImGui::SetCursorScreenPos(ImVec2(nav_top.x, nav_top.y + nav_height));
    const ImGuiWindowFlags page_flags = ImGuiWindowFlags_NoScrollbar;
    if (page_ != drawn_page_) {
        ImGui::SetNextWindowScroll(ImVec2(0.0f, 0.0f));
        drawn_page_ = page_;
    }
    ImGui::BeginChild("##page", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
                      page_flags);
    switch (page_) {
        case Page::Library:  draw_library(console, intent); break;
        case Page::Settings: draw_settings(console, touch_visible, touch_editing,
                                           intent); break;
        case Page::System:   draw_system(console, intent); break;
        case Page::About:    draw_about(); break;
    }
    ImGui::EndChild();
    ImGui::End();

    draw_file_browser(intent);
}

void Ui::draw_library(Console& console, UiIntent& intent) {
    alphabet_filter(library_group_, kCyan, scale_);

    const float rescan_width = 165.0f * scale_;
    const bool narrow_search = ImGui::GetContentRegionAvail().x < 420.0f * scale_;
    ImGui::SetNextItemWidth(narrow_search ? -1.0f :
        ImGui::GetContentRegionAvail().x - rescan_width - ImGui::GetStyle().ItemSpacing.x);
    ImGui::InputTextWithHint("##search", "Search your library...", search_buffer_,
                             sizeof(search_buffer_));
    if (!narrow_search) ImGui::SameLine();
    ImGui::BeginDisabled(games_folder_target_.empty());
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.35f, 0.39f, 1.0f));
    if (ImGui::Button("RESCAN GAMES", ImVec2(narrow_search ? -1.0f : rescan_width, 0.0f))) {
        intent.rescan_games = true;
    }
    ImGui::PopStyleColor();
    ImGui::EndDisabled();

    // The controls above stay fixed. Only the cards are a scrolling surface,
    // and the scrollbar itself is intentionally hidden: touch users swipe the
    // list, while desktop users can still use a wheel or trackpad.
    ImGui::BeginChild("##game_list", ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

    const std::vector<const LibraryGame*> games =
        library_.filtered(search_buffer_, library_group_);
    if (games.empty()) {
        ImGui::BeginChild("##empty_library", ImVec2(0.0f, 180.0f * scale_),
                          ImGuiChildFlags_Borders);
        ImGui::Spacing();
        ImGui::SetWindowFontScale(1.25f);
        ImGui::TextColored(kMuted, library_.games().empty()
                                      ? "YOUR LIBRARY IS EMPTY"
                                      : "NO GAMES MATCH THIS FILTER");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextWrapped(library_.games().empty()
                               ? "Put ISO, BIN/CUE, IMG or CHD images in your configured "
                                 "games folder, then choose Rescan Games."
                               : "Try another letter or clear the search box.");
        ImGui::EndChild();
        ImGui::EndChild();
        return;
    }

    const float available = ImGui::GetContentRegionAvail().x;
    const int columns = available >= 720.0f * scale_
                            ? 4
                            : (available >= 540.0f * scale_
                                   ? 3
                                   : (available >= 400.0f * scale_ ? 2 : 1));
    const float card_height = 150.0f * scale_;

    // Nested child windows and table cells both introduce their own clip
    // channels. On Android's high-DPI SDL renderer those channels can lag a
    // row behind, leaving borders visible but clipping the card contents. One
    // dummy reserves the whole scrollable canvas; cards and buttons are then
    // placed directly in that single canvas with no nested clip state.
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float card_width = (available - gap * (columns - 1)) / columns;
    const size_t row_count =
        (games.size() + static_cast<size_t>(columns) - 1) /
        static_cast<size_t>(columns);
    const float grid_height = card_height * static_cast<float>(row_count) +
                              gap * static_cast<float>(row_count - 1);
    const ImVec2 grid_top = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(available, grid_height));
    const ImVec2 grid_end = ImGui::GetCursorScreenPos();
    const ImVec2 list_clip_min = ImGui::GetWindowPos();
    const ImVec2 list_clip_max(list_clip_min.x + ImGui::GetWindowWidth(),
                               list_clip_min.y + ImGui::GetWindowHeight());
    ImDrawList* const list_draw = ImGui::GetWindowDrawList();
    list_draw->PushClipRect(list_clip_min, list_clip_max, true);

    for (size_t index = 0; index < games.size(); ++index) {
            const LibraryGame& game = *games[index];
            const ImVec4 accent = card_accent(game.name);
            ImGui::PushID(game.target.c_str());

            const size_t row = index / static_cast<size_t>(columns);
            const size_t column = index % static_cast<size_t>(columns);
            const ImVec2 card_top(
                grid_top.x + static_cast<float>(column) * (card_width + gap),
                grid_top.y + static_cast<float>(row) * (card_height + gap));
            const ImVec2 card_bottom(card_top.x + card_width,
                                     card_top.y + card_height);
            if (card_bottom.y < list_clip_min.y || card_top.y > list_clip_max.y) {
                ImGui::PopID();
                continue;
            }
            ImDrawList* draw = list_draw;
            draw->AddRectFilled(card_top, card_bottom,
                                colour(tinted(accent, 0.045f)),
                                ImGui::GetStyle().ChildRounding);
            draw->AddRect(card_top, card_bottom,
                          IM_COL32(static_cast<int>(accent.x * 132.0f),
                                   static_cast<int>(accent.y * 132.0f),
                                   static_cast<int>(accent.z * 132.0f), 224),
                          ImGui::GetStyle().ChildRounding, 0,
                          std::max(1.0f, scale_));
            draw->AddRectFilled(
                card_top, ImVec2(card_top.x + 4.0f * scale_, card_bottom.y),
                colour(accent), ImGui::GetStyle().ChildRounding,
                ImDrawFlags_RoundCornersLeft);

            const float padding = 10.0f * scale_;
            const float content_left = card_top.x + padding;
            const float content_top = card_top.y + padding;
            const float art_height = 76.0f * scale_;
            const float tile_width = 76.0f * scale_;
            const float tile_left =
                card_top.x + (card_width - tile_width) * 0.5f;
            draw->AddRectFilled(
                ImVec2(tile_left, content_top),
                ImVec2(tile_left + tile_width, content_top + art_height),
                colour(tinted(accent, 0.16f)), 6.0f * scale_);
            const char initial[2] = {card_initial(game.name), '\0'};
            const float initial_size = ImGui::GetFontSize() * 1.7f;
            const ImVec2 initial_bounds = ImGui::GetFont()->CalcTextSizeA(
                initial_size, FLT_MAX, 0.0f, initial);
            draw->AddText(
                ImGui::GetFont(), initial_size,
                ImVec2(tile_left +
                           (tile_width - initial_bounds.x) * 0.5f,
                       content_top +
                           (art_height - initial_bounds.y) * 0.5f),
                colour(accent), initial);

            const float text_width = card_width - padding * 2.0f;
            draw_card_title(draw, game.name,
                            ImVec2(content_left,
                                   content_top + art_height + 6.0f * scale_),
                            text_width, IM_COL32(232, 240, 247, 255),
                            ImGui::GetFontSize() * 0.92f);
            const float button_height = ImGui::GetFrameHeight();
            ImGui::SetCursorScreenPos(
                ImVec2(content_left,
                       card_bottom.y - padding - button_height));
            ImGui::BeginDisabled(!console.bios_loaded() || console.demo_loaded());
            ImGui::PushStyleColor(ImGuiCol_Button, tinted(accent, 0.13f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  tinted(accent, 0.30f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                  tinted(accent, 0.46f));
            if (ImGui::Button("PLAY",
                              ImVec2(card_width - padding * 2.0f, 0.0f))) {
                intent.disc_chosen = true;
                intent.start_disc = true;
                intent.disc_path = game.target;
                intent.disc_name = game.name;
                active_discs_ = game.discs;
                active_disc_ = 0;
                session_available_ = true;
                show_launcher_ = false;
                wake_menu_button();
            }
            ImGui::PopStyleColor(3);
            ImGui::EndDisabled();

            if (game.discs.size() > 1) {
                const std::string badge = std::to_string(game.discs.size()) + " DISCS";
                const ImVec2 badge_size = ImGui::GetFont()->CalcTextSizeA(
                    ImGui::GetFontSize() * 0.72f, FLT_MAX, 0.0f, badge.c_str());
                const ImVec2 badge_min(card_bottom.x - badge_size.x -
                                           16.0f * scale_,
                                       card_top.y + 7.0f * scale_);
                draw->AddRectFilled(
                    ImVec2(badge_min.x - 5.0f * scale_, badge_min.y - 2.0f * scale_),
                    ImVec2(card_bottom.x - 6.0f * scale_,
                           badge_min.y + badge_size.y + 2.0f * scale_),
                    IM_COL32(12, 20, 34, 230), 4.0f * scale_);
                draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.72f,
                              badge_min, colour(kAmber), badge.c_str());
            }
            ImGui::PopID();
    }
    list_draw->PopClipRect();
    ImGui::SetCursorScreenPos(grid_end);
    ImGui::Dummy(ImVec2(0.0f, 0.0f));

    if (!console.bios_loaded() || console.demo_loaded()) {
        ImGui::Spacing();
        ImGui::TextColored(
            kAmber,
            console.demo_loaded()
                ? "The built-in demo cannot boot games. Load your 3DO BIOS on the System page."
                : "Load a 3DO BIOS on the System page before playing.");
    }
    ImGui::EndChild();
}

void Ui::draw_settings(Console& console, bool touch_visible, bool touch_editing,
                       UiIntent& intent) {
    page_heading("SETTINGS", "Controls and machine timing");

    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float available_width = ImGui::GetContentRegionAvail().x;
    const bool stacked = available_width < 700.0f * scale_;
    const float column_width = stacked ? available_width : (available_width - gap) * 0.5f;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(10.0f * scale_, 10.0f * scale_));

    begin_panel("##controls_settings", column_width, stacked);
    ImGui::TextColored(kBlue, "ON-SCREEN CONTROLLER");
    ImGui::TextColored(kMuted, "Automatically hidden while a physical pad is connected.");
    if (ImGui::Button(touch_visible ? "ON - HIDE" : "OFF - SHOW",
                      ImVec2(165.0f * scale_, 0.0f))) {
        intent.toggle_touch_controls = true;
    }
    if (touch_visible) {
        same_line_if_fits(touch_editing ? "DONE MOVING" : "EDIT LAYOUT");
        if (ImGui::Button(touch_editing ? "DONE MOVING" : "EDIT LAYOUT")) {
            intent.toggle_layout_edit = true;
        }
        same_line_if_fits("RESET");
        if (ImGui::Button("RESET")) intent.reset_touch_layout = true;
    }

    ImGui::Separator();
    ImGui::TextColored(kGreen, "VIDEO STANDARD");
    int region = console.region() == Region::Pal ? 1 : 0;
    if (ImGui::RadioButton("NTSC 60 Hz", &region, 0)) {
        intent.region_changed = true;
        intent.set_region_pal = false;
    }
    same_line_if_fits("PAL 50 Hz");
    if (ImGui::RadioButton("PAL 50 Hz", &region, 1)) {
        intent.region_changed = true;
        intent.set_region_pal = true;
    }
    end_panel();

    if (!stacked) ImGui::SameLine();
    begin_panel("##machine_settings", column_width, stacked);
    ImGui::TextColored(kAmber, "ARM CPU BOOST");
    ImGui::TextColored(kMuted, "Game CPU only; audio, video and FMV keep native timing.");
    int cpu = static_cast<int>(console.cpu_scale_percent());
    const int speeds[] = {100, 125, 150};
    const char* speed_names[] = {"100%", "125%", "150%"};
    for (int i = 0; i < 3; ++i) {
        if (i != 0) same_line_if_fits(speed_names[i]);
        if (ImGui::RadioButton(speed_names[i], &cpu, speeds[i])) {
            intent.cpu_scale_changed = true;
            intent.cpu_scale_percent = static_cast<u32>(speeds[i]);
        }
    }

    ImGui::Separator();
    ImGui::TextColored(kPink, "PERFORMANCE DISPLAY");
    if (ImGui::Checkbox("Show in-game performance HUD", &performance_visible_)) {
        intent.ui_settings_changed = true;
    }
    end_panel();
    ImGui::PopStyleVar();
}

void Ui::draw_system(Console& console, UiIntent& intent) {
    page_heading("SYSTEM", "Renderer, screen presentation and storage");

    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float available_width = ImGui::GetContentRegionAvail().x;
    const bool stacked = available_width < 700.0f * scale_;
    const float column_width = stacked ? available_width : (available_width - gap) * 0.5f;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(10.0f * scale_, 10.0f * scale_));

    begin_panel("##graphics_system", column_width, stacked);
    ImGui::TextColored(kBlue, "RENDERER");
    int renderer = renderer_backend_;
    if (ImGui::RadioButton("AUTO", &renderer, 0)) {
        renderer_backend_ = 0;
        renderer_startup_message_ = "Renderer change applies next launch";
        intent.ui_settings_changed = true;
    }
    same_line_if_fits("VULKAN");
    if (ImGui::RadioButton("VULKAN", &renderer, 1)) {
        renderer_backend_ = 1;
        renderer_startup_message_ = "Renderer change applies next launch";
        intent.ui_settings_changed = true;
    }
    ImGui::TextColored(kMuted, "Active: %s",
                       actual_renderer_.empty() ? "detecting" : actual_renderer_.c_str());
    if (!renderer_startup_message_.empty()) {
        ImGui::TextColored(kAmber, "%s", renderer_startup_message_.c_str());
    }

    ImGui::Separator();
    ImGui::TextColored(kViolet, "CUSTOM ADRENO / TURNIP DRIVER");
    ImGui::TextColored(gpu_driver_name_.empty() ? kMuted : kGreen, "%s",
                       gpu_driver_name_.empty() ? "System GPU driver"
                                                : gpu_driver_name_.c_str());
    if (AndroidStorage::available()) {
        if (ImGui::Button("IMPORT DRIVER PACKAGE", ImVec2(-1.0f, 0.0f))) {
            intent.import_gpu_driver = true;
        }
        if (!gpu_driver_name_.empty()) {
            if (ImGui::Button("USE SYSTEM DRIVER", ImVec2(-1.0f, 0.0f))) {
                intent.use_system_gpu_driver = true;
            }
        }
        ImGui::TextColored(kMuted,
                           "ADPKG/ZIP is copied to private app storage. Restart to apply.");
    } else {
        ImGui::TextColored(kMuted, "Custom mobile drivers are Android-only.");
    }
    if (!gpu_driver_message_.empty()) {
        ImGui::TextColored(kAmber, "%s", gpu_driver_message_.c_str());
    }

    ImGui::Separator();
    ImGui::TextColored(kPink, "DISPLAY");
    int aspect = widescreen_ ? 1 : 0;
    if (ImGui::RadioButton("4:3 ORIGINAL", &aspect, 0)) {
        widescreen_ = false;
        intent.ui_settings_changed = true;
    }
    same_line_if_fits("16:9 WIDE");
    if (ImGui::RadioButton("16:9 WIDE", &aspect, 1)) {
        widescreen_ = true;
        intent.ui_settings_changed = true;
    }
    if (ImGui::Checkbox("BEZEL", &bezel_)) intent.ui_settings_changed = true;
    same_line_if_fits("CRT EFFECT");
    if (ImGui::Checkbox("CRT EFFECT", &crt_effect_)) {
        intent.ui_settings_changed = true;
    }
    end_panel();

    if (!stacked) ImGui::SameLine();
    begin_panel("##storage_system", column_width, stacked);
    ImGui::TextColored(kAmber, "3DO SYSTEM ROM");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputTextWithHint("##bios", "ROM or BIN path", bios_path_buffer_,
                                 sizeof(bios_path_buffer_))) {
        bios_target_.clear();
    }
    if (ImGui::Button("BROWSE##bios", ImVec2(column_width * 0.48f, 0.0f))) {
        browsing_ = Browsing::Bios;
        browser_.open("Choose a BIOS image", {".rom", ".bin"});
    }
    ImGui::SameLine();
    if (ImGui::Button("LOAD##bios", ImVec2(-1.0f, 0.0f))) {
        intent.bios_chosen = true;
        intent.bios_path = bios_target_.empty() ? std::string(bios_path_buffer_)
                                                 : bios_target_;
        intent.bios_name = bios_path_buffer_;
    }
    ImGui::TextColored(console.bios_loaded() ? kGreen : kAmber,
                       console.demo_loaded()
                           ? "Built-in original demo ROM loaded"
                           : (console.bios_loaded() ? "BIOS loaded - system ready"
                                                   : "No BIOS loaded"));

    ImGui::Separator();
    ImGui::TextColored(kGreen, "INSERTED DISC");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputTextWithHint("##disc", "ISO, BIN/CUE, IMG or CHD",
                                 disc_path_buffer_, sizeof(disc_path_buffer_))) {
        disc_target_.clear();
    }
    if (ImGui::Button("BROWSE##disc", ImVec2(column_width * 0.48f, 0.0f))) {
        browsing_ = Browsing::Disc;
        browser_.open("Choose a disc image", {".iso", ".bin", ".cue", ".img", ".chd"});
    }
    ImGui::SameLine();
    if (ImGui::Button("INSERT##disc", ImVec2(-1.0f, 0.0f))) {
        intent.disc_chosen = true;
        intent.disc_path = disc_target_.empty() ? std::string(disc_path_buffer_)
                                                 : disc_target_;
        intent.disc_name = disc_path_buffer_;
    }
    if (console.disc_loaded()) {
        const Disc& disc = console.disc();
        ImGui::TextColored(kGreen, "Disc ready - %u sectors / %zu track%s",
                           disc.sector_count(), disc.tracks().size(),
                           disc.tracks().size() == 1 ? "" : "s");
        if (ImGui::Button("EJECT")) intent.eject = true;
        same_line_if_fits("START CURRENT DISC");
        ImGui::BeginDisabled(!console.bios_loaded() || console.demo_loaded());
        if (ImGui::Button("START CURRENT DISC")) {
            intent.reset = true;
            session_available_ = true;
            show_launcher_ = false;
            wake_menu_button();
        }
        ImGui::EndDisabled();
    } else {
        ImGui::TextColored(kMuted, "No disc inserted");
    }

    ImGui::Separator();
    ImGui::TextColored(kCyan, "STORAGE FOLDERS");
    ImGui::Text("BIOS: %.42s", bios_folder_name_.empty()
                                   ? "Not configured" : bios_folder_name_.c_str());
    ImGui::Text("Games: %.41s", games_folder_name_.empty()
                                    ? "Not configured" : games_folder_name_.c_str());
    if (ImGui::Button("RUN FOLDER SETUP", ImVec2(-1.0f, 0.0f))) {
        setup_complete_ = false;
        wizard_step_ = WizardStep::Welcome;
    }
    end_panel();
    ImGui::PopStyleVar();
}

void Ui::draw_about() {
    page_heading("ABOUT RETRO-3DO", "A new, mobile-first 3DO emulator");

    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float available_width = ImGui::GetContentRegionAvail().x;
    const bool stacked = available_width < 700.0f * scale_;
    const float column_width = stacked ? available_width : (available_width - gap) * 0.5f;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(10.0f * scale_, 10.0f * scale_));

    begin_panel("##about_story", column_width, stacked);
    ImGui::SetWindowFontScale(1.35f);
    ImGui::TextColored(kCyan, "RETRO-3DO");
    ImGui::SetWindowFontScale(0.88f);
    ImGui::TextColored(kMuted, "Version 2.1.1  |  September 2026");
    ImGui::Spacing();
    ImGui::TextWrapped(
        "Retro-3DO is a brand-new 3DO emulator implementation, designed and "
        "built as its own modern C++ codebase for phones, handhelds, tablets "
        "and desktops. It is not a re-skinned legacy core.");
    ImGui::Spacing();
    ImGui::TextColored(kGreen, "BUILT FOR MODERN HARDWARE");
    ImGui::TextWrapped(
        "The ARM60 CPU uses a decoded-instruction cache, emulation runs on a "
        "dedicated paced thread, and rendering is presented independently. "
        "Android supports Vulkan, custom Adreno drivers, scoped storage and "
        "up to 120 Hz display presentation without changing game or audio speed.");
    ImGui::Spacing();
    ImGui::TextColored(kAmber, "ACTIVE DEVELOPMENT");
    ImGui::TextWrapped(
        "Commercial games including Road Rash and The Need for Speed reach "
        "correctly rendered gameplay. Compatibility continues to expand; "
        "save states, CD audio and less-used hardware paths remain future work.");
    end_panel();

    if (!stacked) ImGui::SameLine();
    begin_panel("##about_technology", column_width, stacked);
    ImGui::SetWindowFontScale(0.88f);
    ImGui::TextColored(kViolet, "THE EMULATED SYSTEM");
    ImGui::TextWrapped(
        "Project implementations cover the ARM60, CLIO timing and interrupts, "
        "MADAM/CEL graphics and matrix hardware, VDLP display lists, DSPP "
        "audio, SPORT memory operations, XBUS CD-ROM and chained controllers.");
    ImGui::Spacing();
    ImGui::TextColored(kGreen, "LICENCE & OWNERSHIP");
    ImGui::TextWrapped(
        "Project-owned code: MIT License. Copyright (c) 2026 Crown Park "
        "Computing. MIT permits use, modification, distribution and sale; "
        "bundled components retain their own permissive licences.");
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextWrapped(
        "Licence notices do not replace third-party rights clearance; see "
        "NOTICE and PROVENANCE.");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::TextColored(kBlue, "DEVELOPED WITH EVIDENCE");
    ImGui::TextWrapped(
        "The core is backed by 250+ focused regression checks, commercial-game "
        "traces, official 3DO and ARM documentation, published hardware patents "
        "and device testing. Opera/FreeDO was also consulted historically as a "
        "compatibility reference; the project does not claim clean-room history.");
    ImGui::Spacing();
    ImGui::TextColored(kPink, "INCLUDED FEATURES");
    ImGui::TextWrapped(
        "ISO/BIN/CUE/IMG/CHD | live multi-disc swap | gamepad and touch "
        "controls | shared mobile and desktop core");
    end_panel();
    ImGui::PopStyleVar();
}

void Ui::open_quick_menu(bool emulating, UiIntent& intent) {
    quick_menu_ = true;
    resume_after_menu_ = emulating;
    if (emulating) intent.toggle_pause = true;
}

void Ui::draw_overlay(Console& console, bool emulating, double display_fps,
                      double emulated_fps, double frame_ms, u64 underruns,
                      bool touch_visible, bool touch_editing, UiIntent& intent) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    constexpr u64 kMenuVisibleMs = 3000;
    const bool show_menu_button =
        touch_editing ||
        SDL_GetTicks() - last_pointer_activity_ms_ < kMenuVisibleMs;
    if (show_menu_button && !quick_menu_) {
        // Top-centre is deliberately outside both shoulder-button hit areas.
        // The old top-left position covered L on handheld layouts.
        ImGui::SetNextWindowPos(
            ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                   viewport->WorkPos.y + 12.0f * scale_),
            ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.72f);
        ImGui::Begin("##menu_button", nullptr,
                     ImGuiWindowFlags_NoDecoration |
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
        if (ImGui::Button("MENU", ImVec2(92.0f * scale_, 0.0f))) {
            open_quick_menu(emulating, intent);
        }
        if (touch_editing) {
            ImGui::SameLine();
            if (ImGui::Button("DONE MOVING")) intent.toggle_layout_edit = true;
        }
        ImGui::End();
    }

    if (!performance_visible_) return;

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 12.0f * scale_,
                                  viewport->WorkPos.y + 68.0f * scale_));
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::Begin("##performance", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoNav);
    const double target = console.region() == Region::Pal ? 50.0 : 60.0;
    ImGui::TextColored(emulated_fps >= target - 2.0 ? kGreen : kAmber,
                       "Machine %.0f / %.0f fps", emulated_fps, target);
    ImGui::Text("Display %.0f fps   Frame %.2f ms", display_fps, frame_ms);
    ImGui::Text("ARM %u%%   PC %08X", console.cpu_scale_percent(),
                console.cpu().pc());
    if (underruns > 0) {
        ImGui::TextColored(kRed, "Audio gaps %llu",
                           static_cast<unsigned long long>(underruns));
    }
    ImGui::End();
}

void Ui::draw_quick_menu(Console& console, bool emulating, bool touch_visible,
                         bool touch_editing, UiIntent& intent) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        viewport->Pos,
        ImVec2(viewport->Pos.x + viewport->Size.x,
               viewport->Pos.y + viewport->Size.y),
        IM_COL32(2, 5, 12, 180));

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                  viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(
        ImVec2(std::min(465.0f * scale_, viewport->WorkSize.x * 0.88f),
               std::min(510.0f * scale_, viewport->WorkSize.y * 0.92f)),
        ImGuiCond_Always);
    ImGui::Begin("##quick_menu", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextColored(kCyan, "GAME MENU");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextColored(kMuted, "Game paused - machine timing is preserved");
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.35f, 0.28f, 1.0f));
    if (wide_button("RESUME", 42.0f * scale_)) {
        quick_menu_ = false;
        if (resume_after_menu_ && !emulating) intent.toggle_pause = true;
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::TextColored(kCyan, "ON-SCREEN CONTROLLER");
    if (ImGui::Checkbox("Enabled", &touch_visible)) {
        intent.toggle_touch_controls = true;
    }
    same_line_if_fits(touch_editing ? "DONE MOVING" : "EDIT LAYOUT");
    ImGui::BeginDisabled(!touch_visible);
    if (ImGui::Button(touch_editing ? "DONE MOVING" : "EDIT LAYOUT")) {
        intent.toggle_layout_edit = true;
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::TextColored(kCyan, "ARM CPU BOOST");
    int cpu = static_cast<int>(console.cpu_scale_percent());
    const int values[] = {100, 125, 150};
    const char* labels[] = {"100%", "125%", "150%"};
    for (int i = 0; i < 3; ++i) {
        if (i != 0) same_line_if_fits(labels[i]);
        if (ImGui::RadioButton(labels[i], &cpu, values[i])) {
            intent.cpu_scale_changed = true;
            intent.cpu_scale_percent = static_cast<u32>(values[i]);
        }
    }
    ImGui::TextColored(kMuted, "Audio, video and FMV remain at native speed.");

    if (active_discs_.size() > 1) {
        ImGui::Spacing();
        ImGui::TextColored(kAmber, "DISC SELECT");
        for (size_t i = 0; i < active_discs_.size(); ++i) {
            const std::string label = "DISC " + std::to_string(i + 1);
            if (i != 0) same_line_if_fits(label.c_str());
            ImGui::BeginDisabled(i == active_disc_);
            if (ImGui::Button(label.c_str())) {
                intent.disc_chosen = true;
                intent.disc_path = active_discs_[i].target;
                intent.disc_name = active_discs_[i].name;
                active_disc_ = i;
            }
            ImGui::EndDisabled();
        }
        ImGui::TextColored(kMuted,
                           "Swap when the game asks; the 3DO is not reset.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("RESET GAME", ImVec2(-1.0f, 0.0f))) {
        intent.reset = true;
        quick_menu_ = false;
    }
    // A demonstration the walkthrough started has no way out to the library:
    // it goes back to the walkthrough, which is what the user was doing.
    if (guide_demo_) {
        if (ImGui::Button("BACK TO THE GUIDE", ImVec2(-1.0f, 0.0f))) {
            guide_demo_ = false;
            show_launcher_ = true;
            quick_menu_ = false;
            intent.eject = true;
            open_guide();
            // Back where they were, not at step one again.
            guide_step_ = guide_demo_step_;
        }
    } else if (ImGui::Button("RETURN TO LIBRARY", ImVec2(-1.0f, 0.0f))) {
        page_ = Page::Library;
        show_launcher_ = true;
        quick_menu_ = false;
    }
#if !RETRO3DO_PLATFORM_IOS
    if (ImGui::Button("EXIT RETRO-3DO", ImVec2(-1.0f, 0.0f))) intent.quit = true;
#endif
    ImGui::PopTextWrapPos();
    ImGui::End();
}

void Ui::draw_file_browser(UiIntent& intent) {
    std::string picked;
    std::string picked_name;
    if (!browser_.draw(&picked, &picked_name)) return;

    if (browsing_ == Browsing::Bios) {
        std::snprintf(bios_path_buffer_, sizeof(bios_path_buffer_), "%s",
                      picked_name.empty() ? picked.c_str() : picked_name.c_str());
        bios_target_ = picked;
        intent.bios_chosen = true;
        intent.bios_path = picked;
        intent.bios_name = picked_name;
    } else if (browsing_ == Browsing::GamesFolder) {
        intent.games_folder_chosen = true;
        intent.folder_path = picked;
        intent.folder_name = picked_name;
    } else if (browsing_ == Browsing::Disc) {
        std::snprintf(disc_path_buffer_, sizeof(disc_path_buffer_), "%s",
                      picked_name.empty() ? picked.c_str() : picked_name.c_str());
        disc_target_ = picked;
        intent.disc_chosen = true;
        intent.disc_path = picked;
        intent.disc_name = picked_name;
    }
    browsing_ = Browsing::None;
}

void Ui::release_ui_texture() {
    SDL_DestroyTexture(ui_texture_);
    ui_texture_ = nullptr;
    ui_texture_width_ = ui_texture_height_ = 0;
}

void Ui::render(SDL_Renderer* renderer) {
    if (!initialised_) return;
    const char* backend = SDL_GetRendererName(renderer);
    if (!backend || std::strcmp(backend, "vulkan") != 0) {
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        return;
    }

    // SDL's Vulkan swapchain scissor does not follow rotated Android surfaces.
    // Texture targets have identity rotation: clip the UI there, then composite
    // it in a single full-frame draw, preserving the game underneath overlays.
    int width = 0, height = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &width, &height);
    if (width != ui_texture_width_ || height != ui_texture_height_) {
        release_ui_texture();
    }
    if (!ui_texture_ && width > 0 && height > 0) {
        ui_texture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_TARGET, width, height);
        if (ui_texture_) {
            ui_texture_width_ = width;
            ui_texture_height_ = height;
            // Drawing onto transparent pixels has already multiplied RGB by A.
            if (!SDL_SetTextureBlendMode(ui_texture_, SDL_BLENDMODE_BLEND_PREMULTIPLIED))
                release_ui_texture();
        }
    }
    SDL_Texture* previous_target = SDL_GetRenderTarget(renderer);
    if (!ui_texture_ || !SDL_SetRenderTarget(renderer, ui_texture_)) {
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        return;
    }
    SDL_FColor previous_colour;
    SDL_GetRenderDrawColorFloat(renderer, &previous_colour.r, &previous_colour.g,
                               &previous_colour.b, &previous_colour.a);
    SDL_SetRenderViewport(renderer, nullptr);
    SDL_SetRenderClipRect(renderer, nullptr);
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_SetRenderTarget(renderer, previous_target);
    // Viewport, clip and scale are restored by SDL with the previous target.
    SDL_RenderTexture(renderer, ui_texture_, nullptr, nullptr);
    SDL_SetRenderDrawColorFloat(renderer, previous_colour.r, previous_colour.g,
                               previous_colour.b, previous_colour.a);
}

// ---------------------------------------------------------------------------
// The walkthrough
//
// Shown once on a first run and reachable from GUIDE for ever after. It is
// modal on purpose: a first-time user being talked through the app should not
// be able to half-start a game underneath it.
// ---------------------------------------------------------------------------
void Ui::draw_guide(UiIntent& intent) {
    if (guide_resume_ && browsing_ == Browsing::None) {
        guide_resume_ = false;
        guide_open_ = true;
        guide_focus_ = true;
    }
    if (!guide_open_) return;

    // Android grants a folder and the desktop points at one; the platform
    // decides which of those steps exists at all, so the count differs.
#if defined(__ANDROID__)
    const unsigned platform = kGuideAndroid;
    const char* add_control = "Add disc";
#else
    const unsigned platform = kGuideDesktop;
    const char* add_control = "Add disc";
#endif
    const int steps = static_cast<int>(guide_step_count(platform));
    if (guide_step_ < 0 || guide_step_ >= steps) { guide_open_ = false; return; }
    const GuideStep* step = guide_step(platform, static_cast<std::size_t>(guide_step_));
    if (step == nullptr) { guide_open_ = false; return; }

    // The step's own colour, from the shared table, so the walkthrough looks
    // the same here as it does on a phone.
    const ImVec4 accent(static_cast<float>((step->accent >> 16) & 0xFF) / 255.0f,
                        static_cast<float>((step->accent >> 8) & 0xFF) / 255.0f,
                        static_cast<float>(step->accent & 0xFF) / 255.0f, 1.0f);

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    if (guide_focus_) {
        ImGui::SetNextWindowFocus();
        guide_focus_ = false;
    }
    ImGui::Begin("##guide_backdrop", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(viewport->WorkPos,
                        ImVec2(viewport->WorkPos.x + viewport->WorkSize.x,
                               viewport->WorkPos.y + viewport->WorkSize.y),
                        IM_COL32(6, 10, 22, 242));

    const float width = std::min(720.0f * scale_, viewport->WorkSize.x * 0.92f);
    const float height = std::min(430.0f * scale_, viewport->WorkSize.y * 0.90f);
    ImGui::SetCursorPos(ImVec2((viewport->WorkSize.x - width) * 0.5f,
                               (viewport->WorkSize.y - height) * 0.5f));
    ImGui::BeginChild("##guide_card", ImVec2(width, height),
                      ImGuiChildFlags_Borders);
    ImGui::PushTextWrapPos(0.0f);

    // A row of bars rather than "3 of 9": it shows how much is left without
    // asking anyone to count, and it is where the colour reads most clearly.
    const float bar_w = (ImGui::GetContentRegionAvail().x -
                         ImGui::GetStyle().ItemSpacing.x * (steps - 1)) / steps;
    for (int i = 0; i < steps; ++i) {
        if (i > 0) ImGui::SameLine();
        const GuideStep* s_i = guide_step(platform, static_cast<std::size_t>(i));
        ImVec4 c = kMuted;
        if (s_i != nullptr && i <= guide_step_) {
            c = ImVec4(static_cast<float>((s_i->accent >> 16) & 0xFF) / 255.0f,
                       static_cast<float>((s_i->accent >> 8) & 0xFF) / 255.0f,
                       static_cast<float>(s_i->accent & 0xFF) / 255.0f, 1.0f);
        }
        ImGui::PushStyleColor(ImGuiCol_ChildBg, c);
        ImGui::BeginChild(ImGui::GetID(static_cast<void*>(nullptr) + i + 1),
                          ImVec2(bar_w, 4.0f * scale_));
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();
    ImGui::TextColored(kMuted, "STEP %d OF %d", guide_step_ + 1, steps);
    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextColored(accent, "%s", step->title);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();
    ImGui::Spacing();

    // "Add disc" rather than iOS's "+ tile": the shared text leaves a token so
    // each front end can name its own control.
    char body[4096];
    guide_body(step->body, add_control, body, sizeof body);
    // The wrap position has to be pushed INSIDE the child. BeginChild starts a
    // new window and the outer PushTextWrapPos does not reach into it, so the
    // text ran straight off the right edge and was clipped rather than wrapped.
    // Leave room for the navigation row, and for the action row when the step
    // has one -- otherwise the buttons it offers are pushed off the card.
    const float reserved = (step->action != kActionNone ? 104.0f : 56.0f) * scale_;
    ImGui::BeginChild("##guide_body", ImVec2(0.0f, -reserved));
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(body);
    ImGui::PopTextWrapPos();
    ImGui::EndChild();

    // What the step can do, offered as buttons rather than described. A file
    // picker needs the guide out of the way, so it hides and resumes.
    auto perform = [&](GuideAction action) {
        switch (action) {
            case kActionPlayDemo:
                intent.start_demo = true;
                session_available_ = true;
                show_launcher_ = false;
                guide_open_ = false;
                guide_demo_ = true;
                guide_demo_step_ = guide_step_;
                wake_menu_button();
                break;
            case kActionAddGame:
                browsing_ = Browsing::Disc;
                browser_.open("Choose a game", {".iso", ".chd", ".img", ".cue", ".bin"});
                guide_open_ = false;
                guide_resume_ = true;
                break;
            case kActionAddBios:
                browsing_ = Browsing::Bios;
                browser_.open("Choose your 3DO system ROM", {".rom", ".bin"});
                guide_open_ = false;
                guide_resume_ = true;
                break;
            case kActionChooseFolder:
                if (AndroidStorage::available()) {
                    wizard_pick_ = WizardPick::Games;
                    AndroidStorage::pick_folder();
                    guide_open_ = false;
                    guide_resume_ = true;
                } else {
                    browsing_ = Browsing::GamesFolder;
                    browser_.open_folder("Choose the folder your games are in");
                    guide_open_ = false;
                    guide_resume_ = true;
                }
                break;
            case kActionNone:
                break;
        }
    };

    if (step->action != kActionNone && step->action_label != nullptr) {
        ImGui::PushStyleColor(ImGuiCol_Button, tinted(accent, 0.34f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, tinted(accent, 0.50f));
        const bool wide = step->action2 == kActionNone;
        if (ImGui::Button(step->action_label,
                          ImVec2(wide ? -1.0f : ImGui::GetContentRegionAvail().x * 0.5f -
                                        ImGui::GetStyle().ItemSpacing.x,
                                 40.0f * scale_))) {
            perform(step->action);
        }
        ImGui::PopStyleColor(2);
        if (!wide && step->action2_label != nullptr) {
            ImGui::SameLine();
            if (ImGui::Button(step->action2_label, ImVec2(-1.0f, 40.0f * scale_))) {
                perform(step->action2);
            }
        }
        ImGui::Spacing();
    }

    const float button = 150.0f * scale_;
    if (guide_step_ > 0) {
        if (ImGui::Button("BACK", ImVec2(button, 40.0f * scale_))) --guide_step_;
        ImGui::SameLine();
    }
    if (guide_step_ < steps - 1) {
        if (ImGui::Button("SKIP", ImVec2(button, 40.0f * scale_))) guide_open_ = false;
        ImGui::SameLine();
    }
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - button - ImGui::GetStyle().WindowPadding.x);
    const char* next = guide_step_ == steps - 1 ? "START PLAYING" : "NEXT";
    ImGui::PushStyleColor(ImGuiCol_Button, tinted(accent, 0.42f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, tinted(accent, 0.58f));
    const bool advance = ImGui::Button(next, ImVec2(button, 40.0f * scale_));
    ImGui::PopStyleColor(2);
    if (advance) {
        if (guide_step_ == steps - 1) guide_open_ = false; else ++guide_step_;
    }

    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    ImGui::End();
}

}  // namespace retro3do
