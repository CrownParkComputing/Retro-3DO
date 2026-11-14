// Retro-3DO front end: splash, library, settings and in-game quick menu.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/types.h"
#include "ui/file_browser.h"
#include "ui/game_library.h"

struct ImGuiStyle;
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
union SDL_Event;

namespace retro3do {

class Console;

// What the UI asks the application to do. It is consumed once per display
// frame, keeping storage and machine mutations out of the drawing code.
struct UiIntent {
    bool quit = false;
    bool reset = false;
    bool start_demo = false;
    bool toggle_pause = false;
    bool bios_chosen = false;
    std::string bios_path;
    std::string bios_name;
    bool disc_chosen = false;
    bool start_disc = false;
    std::string disc_path;
    std::string disc_name;
    bool eject = false;
    bool toggle_touch_controls = false;
    bool toggle_layout_edit = false;
    bool reset_touch_layout = false;
    bool region_changed = false;
    bool set_region_pal = false;
    bool cpu_scale_changed = false;
    u32 cpu_scale_percent = 100;
    bool ui_settings_changed = false;
    bool bios_folder_chosen = false;
    bool games_folder_chosen = false;
    bool setup_finished = false;
    bool rescan_games = false;
    bool import_gpu_driver = false;
    bool use_system_gpu_driver = false;
    std::string folder_path;
    std::string folder_name;
};

class Ui {
public:
    Ui();
    ~Ui();

    Ui(const Ui&) = delete;
    Ui& operator=(const Ui&) = delete;

    bool init(SDL_Window* window, SDL_Renderer* renderer);
    void hide_launcher();
    void show_launcher() { show_launcher_ = true; }

    void set_remembered_bios(const std::string& name, const std::string& target);
    void set_remembered_disc(const std::string& name, const std::string& target);
    void set_game_library(const std::vector<LibraryGame>& games);
    void configure_setup(bool complete, const std::string& bios_folder_name,
                         const std::string& bios_folder_target,
                         const std::string& games_folder_name,
                         const std::string& games_folder_target);
    void set_performance_visible(bool visible) { performance_visible_ = visible; }
    bool performance_visible() const { return performance_visible_; }
    void configure_video(int renderer_backend, bool widescreen, bool bezel,
                         bool crt_effect, const std::string& gpu_driver_name);
    void set_renderer_status(const std::string& actual,
                             const std::string& startup_message);
    void set_gpu_driver_status(const std::string& name,
                               const std::string& message);
    int renderer_backend() const { return renderer_backend_; }
    bool widescreen() const { return widescreen_; }
    bool bezel() const { return bezel_; }
    bool crt_effect() const { return crt_effect_; }
    const std::string& gpu_driver_name() const { return gpu_driver_name_; }
    void suspend_renderer();
    bool resume_renderer(SDL_Renderer* renderer);
    void shutdown();

    void process_event(const SDL_Event& event);
    bool wants_mouse() const;
    bool wants_keyboard() const;

    UiIntent build(Console& console, bool emulating, double display_fps,
                   double emulated_fps, double frame_ms, u64 underruns,
                   bool touch_visible, bool touch_editing);
    void render(SDL_Renderer* renderer);

private:
    enum class Page { Library, Settings, System, About };
    enum class Browsing { None, Bios, Disc };
    enum class WizardStep { Welcome, Bios, Games, Finish };
    enum class WizardPick { None, Bios, Games };

    void release_ui_texture();
    void update_scale();
    void apply_safe_area();
    void update_touch_scroll();
    void draw_splash();
    void draw_setup_wizard(Console& console, UiIntent& intent);
    void poll_setup_picker(UiIntent& intent);
    void draw_launcher(Console& console, bool emulating, bool touch_visible,
                       bool touch_editing, UiIntent& intent);
    void draw_library(Console& console, UiIntent& intent);
    void draw_settings(Console& console, bool touch_visible, bool touch_editing,
                       UiIntent& intent);
    void draw_system(Console& console, UiIntent& intent);
    void draw_about();
    // The shared walkthrough, drawn over whatever is behind it. The words come
    // from src/core/guide.cpp so that this and the iOS app say the same thing.
    void draw_guide(UiIntent& intent);
    void open_guide() { guide_open_ = true; guide_step_ = 0; }
    void draw_overlay(Console& console, bool emulating, double display_fps,
                      double emulated_fps, double frame_ms, u64 underruns,
                      bool touch_visible, bool touch_editing, UiIntent& intent);
    void draw_quick_menu(Console& console, bool emulating, bool touch_visible,
                         bool touch_editing, UiIntent& intent);
    void open_quick_menu(bool emulating, UiIntent& intent);
    void wake_menu_button();
    void draw_file_browser(UiIntent& intent);
    void load_splash_texture();

    FileBrowser browser_;
    GameLibrary library_;
    Browsing browsing_ = Browsing::None;
    Page page_ = Page::Library;
    Page drawn_page_ = Page::Library;

    SDL_Window* window_ = nullptr;
    std::unique_ptr<ImGuiStyle> base_style_;
    float scale_ = 1.0f;
    bool initialised_ = false;
    bool show_launcher_ = true;
    bool setup_complete_ = true;
    bool session_available_ = false;
    bool splash_complete_ = false;
    bool quick_menu_ = false;
    bool resume_after_menu_ = false;
    bool performance_visible_ = false;
    int renderer_backend_ = 0;
    bool widescreen_ = false;
    bool bezel_ = false;
    bool crt_effect_ = false;
    bool menu_requested_ = false;
    u64 splash_started_ms_ = 0;
    u64 last_pointer_activity_ms_ = 0;
    u32 scroll_window_ = 0;
    float scroll_start_y_ = 0.0f;
    float scroll_start_offset_ = 0.0f;
    bool scroll_dragging_ = false;
    char search_buffer_[128] = {};
    char library_group_ = 0;
    WizardStep wizard_step_ = WizardStep::Welcome;
    bool guide_open_ = false;
    bool guide_seen_ = false;
    int  guide_step_ = 0;
    // Set when a step sends the user off to pick a file. The guide hides
    // itself so the browser is visible, then comes back where it left off --
    // being thrown out of a walkthrough for doing what it asked is worse than
    // not offering the button at all.
    bool guide_resume_ = false;
    // A demonstration the walkthrough started. Its only way out is back to the
    // walkthrough, so the quick menu offers that instead of the library.
    bool guide_demo_ = false;
    int  guide_demo_step_ = 0;
    WizardPick wizard_pick_ = WizardPick::None;
    std::string bios_folder_name_;
    std::string bios_folder_target_;
    std::string games_folder_name_;
    std::string games_folder_target_;
    std::string actual_renderer_;
    std::string renderer_startup_message_;
    std::string gpu_driver_name_;
    std::string gpu_driver_message_;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* ui_texture_ = nullptr;
    int ui_texture_width_ = 0;
    int ui_texture_height_ = 0;
    SDL_Texture* splash_texture_ = nullptr;

    std::vector<LibraryDisc> active_discs_;
    size_t active_disc_ = 0;

    char bios_path_buffer_[512] = {};
    char disc_path_buffer_[512] = {};
    std::string bios_target_;
    std::string disc_target_;
};

}  // namespace retro3do
