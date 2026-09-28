/*
 * touch_pad.h -- the on-screen controller.
 *
 * The C++ half of retro_touch_pad, the family's shared touch controller. Same
 * ideas and the same layout file as the Dart package, so an arrangement made
 * in one app means the same thing in another:
 *
 *   profile  what a machine has: clusters of buttons with default places, and
 *            a stick or d-pad. Data, not code -- one overlay draws every one.
 *   layout   where the player put them: fractions of the play area (a phone
 *            held either way and a tablet agree), a scale and a hidden flag
 *            per cluster, plus any extra buttons they added.
 *   overlay  draws a layout over the picture and turns fingers into presses.
 *            Directions are merged -- the stick and any "UP as a button" are
 *            combined here, so the host never sees two sources fighting over
 *            the same joystick bits.
 *
 * The host supplies one sink: pressed(action id, down) and directions(u,d,l,r).
 * Everything else is inside.
 */
#ifndef RETRO3DO_RETRO_TOUCH_PAD_H
#define RETRO3DO_RETRO_TOUCH_PAD_H

#include <SDL3/SDL.h>
#include <imgui.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace touchpad {

/* ---- the model -------------------------------------------------------- */

enum class Shape { Column, Row, Diamond, Grid3x2 };
enum class Face  { Circle, Pill, Square };
enum class Stick { Wobble, Dpad };

struct Button {
    std::string id;      /* what the host switches on: "a", "fire1", "lb" */
    std::string label;
    ImU32 colour;
    float size;          /* logical pixels at scale 1 */
    Face  face;
};

struct Cluster {
    std::string id;
    std::string label;
    float dx, dy;        /* default centre, as fractions */
    bool  is_stick;
    float stick_size;    /* diameter at scale 1 */
    Shape shape;
    std::vector<Button> buttons;
};

struct Profile {
    std::string id;      /* stable: it names the layout file */
    std::string name;
    Stick default_stick;
    bool  allow_direction_buttons;
    std::vector<Cluster> clusters;
    const Cluster *cluster(const std::string &id) const;
};

/* The pads on offer. Ids match the Dart package's profiles. */
const Profile &profile_xbox360();
const Profile &profile_generic();     /* stick + two fire buttons */
const Profile &profile_saturn();
const Profile &profile_dos();       /* stick + two fire, Enter/Esc -- DOS default */
const Profile &profile_3do();       /* D-pad, A/B/C, L/R, Stop/Play */
const Profile *profile_by_id(const std::string &id);

struct Placed {
    float dx, dy;
    float scale = 1.0f; /* size multiplier; user-tunable per cluster */
    float spacing = 1.0f; /* gap between buttons INSIDE this cluster,
                          * multiplied into the shape's inter-button
                          * gap (Column / Row / Diamond side / Grid3x2).
                          * 1.0 is the default; >1 spreads the buttons
                          * apart (useful when a cluster has grown with
                          * the size slider and its members are now
                          * crowding one another), <1 packs them
                          * closer. Per-cluster (added 2026-09-18)
                          * because each cluster has its own shape and
                          * its own member spacing concerns. */
    bool visible = true;
};

struct Extra {
    std::string id, label;   /* "dir:up" for a direction, else a host action */
    float dx, dy;
    float scale = 1.0f;
    float spacing = 1.0f; /* per-extra outward push, same idea as
                          * Placed::spacing; kept on Extras so an added
                          * button participates in the same model. */
    bool is_direction() const { return id.rfind("dir:", 0) == 0; }
};

struct Layout {
    std::string profile;
    std::map<std::string, Placed> clusters;
    std::vector<Extra> extras;
    Stick stick = Stick::Wobble;
    float opacity = 0.75f;

    static Layout defaults(const Profile &p);
    /* Anything unreadable, or written for another profile, is the defaults:
     * a corrupt file must never be the reason a game has no controls. */
    static Layout decode(const std::string &json, const Profile &p);
    std::string encode() const;

    /* Written whole to a .tmp and renamed into place. */
    static Layout load(const std::string &dir, const Profile &p);
    bool save(const std::string &dir) const;
    static std::string file_for(const std::string &dir, const Profile &p);
};

/* ---- the overlay ------------------------------------------------------- */

struct Sink {
    std::function<void(bool up, bool down, bool left, bool right)> directions;
    std::function<void(const std::string &id, bool down)> action;
};

class Overlay {
public:
    Overlay() = default;

    /* Switching pad releases everything the old one held. */
    void set(const Profile *p, const Layout &l);
    const Profile *profile() const { return prof_; }
    Layout &layout() { return lay_; }
    const Layout &layout() const { return lay_; }

    /* Arranging: controls drag instead of press; a tap selects one for the
     * size slider. Leaving edit mode releases nothing because nothing was
     * pressed. */
    void set_editing(bool on);
    bool editing() const { return editing_; }
    /* True once a drag has ended since the last call -- the moment to save. */
    bool take_dirty() { const bool d = dirty_; dirty_ = false; return d; }

    /* Feed SDL events. Returns true when the event was for the pad and the
     * host should not also treat it as a gun shot or a mouse click.
     * `window` is the window size in pixels (fingers arrive normalised to
     * it); the area is where the pad is laid out, in window pixels -- the
     * whole window in a game, a preview box on the settings page. Mouse
     * buttons are accepted as one more finger, so the pad can be tried on a
     * desktop. */
    bool handle(const SDL_Event &ev, const ImVec2 &window,
                const ImVec2 &area_pos, const ImVec2 &area_size, const Sink &sink);

    /* Draw into a draw list (the foreground list, over the picture). */
    void draw(ImDrawList *dl, const ImVec2 &area_pos, const ImVec2 &area_size);

    /* The designer's side panel: stick/d-pad, opacity, the selected control's
     * size and visibility, add/remove extra buttons. Returns true when the
     * layout changed and wants saving.
     *
     * Split 2026-09-18 (mirrors Retro-PSX-SDL3): general pad controls
     * (Movement / Opacity / Add a button / Reset layout) stay inline as
     * designer_controls(); cluster-specific controls (Size / Shown /
     * Remove) moved to cluster_panel() so the host can show them in a
     * modal popup instead of stacking them under a preview the user just
     * clicked in. */
    bool designer_controls(const Sink *sink_for_release = nullptr);

    /* The selected-cluster panel: Size slider, Shown checkbox, Remove.
     * Only meaningful when something is selected -- the caller should open
     * the popup only on that condition. Returns true when the layout
     * changed and wants saving. */
    bool cluster_panel();

    /* Whether anything is currently selected (a cluster or an extra).
     * Lets the host decide when to open the popup. */
    bool has_selection() const { return !selected_.empty(); }
    const std::string &selection_id() const { return selected_; }
    void clear_selection() { selected_.clear(); }

    /* Let go of everything held: leaving the game, hiding the pad. */
    void release_all(const Sink &sink);

private:
    struct Pointer { ImVec2 pos; std::string on; bool stick = false; };
    struct Hit { std::string kind; std::string id; ImVec2 centre; float radius; ImVec2 half; };

    void rebuild_hits(const ImVec2 &pos, const ImVec2 &size);
    const Hit *hit_at(const ImVec2 &p) const;
    void recompute(const Sink &sink);
    void press(const std::string &id, bool down, const Sink &sink);
    std::string select_at(const ImVec2 &p) const;

    const Profile *prof_ = nullptr;
    Layout lay_;
    bool editing_ = false;
    bool dirty_ = false;
    std::vector<Hit> hits_;
    std::map<SDL_FingerID, Pointer> pointers_;   /* mouse is finger 0 with a flag */
    bool mouse_down_ = false;
    std::map<std::string, int> held_;             /* action id -> pointer count */
    bool sent_[4] = {false, false, false, false};
    ImVec2 stick_knob_ = ImVec2(0, 0);            /* offset for the wobble knob */
    bool stick_active_ = false;
    std::string selected_;                        /* editing: cluster id or "extra:<id>" */
    std::string dragging_;
    ImVec2 drag_last_;
    ImVec2 last_pos_, last_size_;
};

} /* namespace touchpad */

#endif /* RETRO3DO_RETRO_TOUCH_PAD_H */
