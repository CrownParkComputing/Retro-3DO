/* touch_pad.cpp -- see touch_pad.h. */

#include "retro_touch_pad.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace touchpad {

/* ==================================================================== */
/* Profiles                                                              */
/* ==================================================================== */

namespace {

const ImU32 kRed    = IM_COL32(0xDC, 0x32, 0x32, 255);
const ImU32 kBlue   = IM_COL32(0x30, 0x50, 0xDC, 255);
const ImU32 kGreen  = IM_COL32(0x2E, 0x9E, 0x44, 255);
const ImU32 kYellow = IM_COL32(0xD8, 0xC4, 0x3C, 255);
const ImU32 kGrey   = IM_COL32(0x5A, 0x5A, 0x5A, 255);
const ImU32 kSlate  = IM_COL32(0x6E, 0x76, 0x81, 255);
const ImU32 kAccent = IM_COL32(0x34, 0xD9, 0xC4, 255);

Cluster stick(const char *id, const char *label, float dx, float dy)
{
    Cluster c; c.id = id; c.label = label; c.dx = dx; c.dy = dy;
    c.is_stick = true; c.stick_size = 150.0f; c.shape = Shape::Column;
    return c;
}
Cluster buttons(const char *id, const char *label, float dx, float dy, Shape s,
                std::vector<Button> b)
{
    Cluster c; c.id = id; c.label = label; c.dx = dx; c.dy = dy;
    c.is_stick = false; c.stick_size = 0; c.shape = s; c.buttons = std::move(b);
    return c;
}
Button btn(const char *id, const char *label, ImU32 col, float size = 64,
           Face f = Face::Circle)
{
    return Button{ id, label, col, size, f };
}

} /* namespace */

const Cluster *Profile::cluster(const std::string &cid) const
{
    for (const Cluster &c : clusters) if (c.id == cid) return &c;
    return nullptr;
}

/* The 360 pad most people know. Machine-neutral: the host maps the ids. */
const Profile &profile_xbox360()
{
    static const Profile p = [] {
        Profile p; p.id = "xbox360"; p.name = "360 pad";
        p.default_stick = Stick::Dpad; p.allow_direction_buttons = false;
        p.clusters = {
            stick("dpad", "D-pad", 0.13f, 0.70f),
            buttons("face", "Buttons", 0.86f, 0.68f, Shape::Diamond, {
                btn("y", "Y", kYellow, 58), btn("x", "X", kBlue, 58),
                btn("b", "B", kRed, 58),    btn("a", "A", kGreen, 58) }),
            buttons("shoulder_l", "LB / LT", 0.10f, 0.14f, Shape::Row, {
                btn("lt", "LT", kSlate, 44, Face::Pill), btn("lb", "LB", kSlate, 44, Face::Pill) }),
            buttons("shoulder_r", "RB / RT", 0.90f, 0.14f, Shape::Row, {
                btn("rb", "RB", kSlate, 44, Face::Pill), btn("rt", "RT", kSlate, 44, Face::Pill) }),
            buttons("meta", "Back / Start", 0.50f, 0.92f, Shape::Row, {
                btn("back", "BACK", kGrey, 34, Face::Pill), btn("start", "START", kGrey, 34, Face::Pill) }),
        };
        return p;
    }();
    return p;
}

/* Stick and two fire buttons: the least glass covered. */
const Profile &profile_generic()
{
    static const Profile p = [] {
        Profile p; p.id = "generic"; p.name = "Joystick";
        p.default_stick = Stick::Wobble; p.allow_direction_buttons = true;
        p.clusters = {
            stick("stick", "Stick", 0.13f, 0.74f),
            buttons("fire", "Fire", 0.89f, 0.72f, Shape::Column, {
                btn("fire2", "2", kBlue, 72), btn("fire1", "1", kRed, 72) }),
        };
        return p;
    }();
    return p;
}

/* The real thing: six face buttons in two rows, L/R, Start. */
const Profile &profile_saturn()
{
    static const Profile p = [] {
        Profile p; p.id = "saturn"; p.name = "Saturn pad";
        p.default_stick = Stick::Dpad; p.allow_direction_buttons = false;
        p.clusters = {
            stick("dpad", "D-pad", 0.13f, 0.70f),
            buttons("face", "Buttons", 0.82f, 0.70f, Shape::Grid3x2, {
                btn("x", "X", kSlate, 50), btn("y", "Y", kSlate, 50), btn("z", "Z", kSlate, 50),
                btn("a", "A", kRed, 56),   btn("b", "B", kBlue, 56),  btn("c", "C", kGreen, 56) }),
            buttons("triggers", "L / R", 0.50f, 0.10f, Shape::Row, {
                btn("l", "L", kSlate, 44, Face::Pill), btn("start", "START", kGrey, 34, Face::Pill),
                btn("r", "R", kSlate, 44, Face::Pill) }),
        };
        return p;
    }();
    return p;
}

/* DOS: a stick and two fire buttons, plus Enter and Esc as pill buttons on
 * the bottom row. Directions can be added as buttons (arrow-key games), and
 * the host maps every id to a scancode or the joystick. */
const Profile &profile_dos()
{
    static const Profile p = [] {
        Profile p; p.id = "dos"; p.name = "DOS";
        p.default_stick = Stick::Wobble; p.allow_direction_buttons = true;
        p.clusters = {
            stick("stick", "Stick", 0.13f, 0.72f),
            buttons("fire", "Fire", 0.89f, 0.70f, Shape::Column, {
                btn("fire2", "2", kBlue, 72), btn("fire1", "1", kRed, 72) }),
            buttons("meta", "Enter / Esc", 0.50f, 0.92f, Shape::Row, {
                btn("start", "ENTER", kGrey, 36, Face::Pill),
                btn("back", "ESC", kGrey, 36, Face::Pill) }),
        };
        return p;
    }();
    return p;
}

const Profile *profile_by_id(const std::string &id)
{
    if (id == "3do") return &profile_3do();
    if (id == "xbox360") return &profile_xbox360();
    if (id == "generic") return &profile_generic();
    if (id == "saturn")  return &profile_saturn();
    if (id == "dos")     return &profile_dos();
    return nullptr;
}

const Profile &profile_3do()
{
    static const Profile p = [] {
        Profile p; p.id = "3do"; p.name = "3DO pad";
        p.default_stick = Stick::Dpad; p.allow_direction_buttons = true;
        p.clusters = {
            stick("dpad", "D-pad", 0.14f, 0.73f),
            buttons("face", "A / B / C", 0.82f, 0.73f, Shape::Row, {
                btn("c", "C", kGreen), btn("b", "B", kBlue), btn("a", "A", kRed) }),
            buttons("shoulder_l", "L", 0.12f, 0.16f, Shape::Row, {
                btn("lshift", "L", kSlate, 48, Face::Pill) }),
            buttons("shoulder_r", "R", 0.88f, 0.16f, Shape::Row, {
                btn("rshift", "R", kSlate, 48, Face::Pill) }),
            buttons("meta", "Stop / Play", 0.50f, 0.88f, Shape::Row, {
                btn("stop", "STOP", kGrey, 40, Face::Pill),
                btn("play", "PLAY", kGrey, 40, Face::Pill) }),
        };
        return p;
    }();
    return p;
}

/* ==================================================================== */
/* A very small JSON                                                     */
/* ==================================================================== */

namespace {

struct J {
    enum T { Null, Bool, Num, Str, Arr, Obj } t = Null;
    bool b = false; double n = 0; std::string s;
    std::vector<J> a; std::map<std::string, J> o;
    const J *get(const char *k) const {
        if (t != Obj) return nullptr;
        auto it = o.find(k); return it == o.end() ? nullptr : &it->second;
    }
    double num(double d) const { return t == Num ? n : d; }
    bool boolean(bool d) const { return t == Bool ? b : d; }
    std::string str(const std::string &d) const { return t == Str ? s : d; }
};

struct Parser {
    const std::string &src; size_t i = 0; bool ok = true;
    explicit Parser(const std::string &s) : src(s) {}
    void ws() { while (i < src.size() && isspace((unsigned char)src[i])) ++i; }
    bool eat(char c) { ws(); if (i < src.size() && src[i] == c) { ++i; return true; } return false; }
    J value() {
        ws(); J v;
        if (i >= src.size()) { ok = false; return v; }
        const char c = src[i];
        if (c == '{') {
            ++i; v.t = J::Obj;
            if (eat('}')) return v;
            do {
                ws(); J k = value(); if (!ok || k.t != J::Str) { ok = false; return v; }
                if (!eat(':')) { ok = false; return v; }
                v.o[k.s] = value(); if (!ok) return v;
            } while (eat(','));
            if (!eat('}')) ok = false;
        } else if (c == '[') {
            ++i; v.t = J::Arr;
            if (eat(']')) return v;
            do { v.a.push_back(value()); if (!ok) return v; } while (eat(','));
            if (!eat(']')) ok = false;
        } else if (c == '"') {
            ++i; v.t = J::Str;
            while (i < src.size() && src[i] != '"') {
                if (src[i] == '\\' && i + 1 < src.size()) {
                    ++i;
                    switch (src[i]) {
                    case 'n': v.s += '\n'; break; case 't': v.s += '\t'; break;
                    case 'u': i += 4; v.s += '?'; break;
                    default: v.s += src[i];
                    }
                } else v.s += src[i];
                ++i;
            }
            if (i >= src.size()) { ok = false; return v; }
            ++i;
        } else if (c == 't' && src.compare(i, 4, "true") == 0)  { i += 4; v.t = J::Bool; v.b = true; }
        else if (c == 'f' && src.compare(i, 5, "false") == 0) { i += 5; v.t = J::Bool; v.b = false; }
        else if (c == 'n' && src.compare(i, 4, "null") == 0)  { i += 4; }
        else {
            char *end = nullptr;
            v.n = strtod(src.c_str() + i, &end);
            if (end == src.c_str() + i) { ok = false; return v; }
            i = (size_t)(end - src.c_str()); v.t = J::Num;
        }
        return v;
    }
};

std::string quote(const std::string &s)
{
    std::string o = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else o += c;
    }
    return o + "\"";
}
std::string num(double d)
{
    char b[32]; snprintf(b, sizeof b, "%.4g", d);
    /* strtod on the way back in reads any of these; the locale is never
     * consulted because we format ourselves. */
    return b;
}

const float kMinPos = 0.04f, kMaxPos = 0.96f;
/* 0.5..3.0x: was 0.6..1.6 until 2026-09-18 — 1.6x left the buttons noticeably
 * cramped on a 1920x1080 Retroid screen; the wider ceiling lets one drag cover
 * the "make it all bigger" case without per-cluster fiddling. */
const float kMinScale = 0.5f, kMaxScale = 3.0f;
float clampf(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }

} /* namespace */

/* ==================================================================== */
/* Layout                                                                */
/* ==================================================================== */

Layout Layout::defaults(const Profile &p)
{
    Layout l; l.profile = p.id; l.stick = p.default_stick;
    for (const Cluster &c : p.clusters)
        l.clusters[c.id] = Placed{ c.dx, c.dy, 1.0f, 1.0f, true };
    return l;
}

Layout Layout::decode(const std::string &json, const Profile &p)
{
    Layout d = defaults(p);
    if (json.empty()) return d;
    Parser ps(json);
    const J root = ps.value();
    if (!ps.ok || root.t != J::Obj) return d;
    if (const J *pr = root.get("profile"); !pr || pr->str("") != p.id) return d;

    Layout l = d;
    if (const J *st = root.get("stick"))
        l.stick = st->str("") == "dpad" ? Stick::Dpad
                : st->str("") == "wobble" ? Stick::Wobble : p.default_stick;
    if (const J *op = root.get("opacity")) l.opacity = clampf((float)op->num(0.75), 0.2f, 1.0f);

    auto point = [](const J *j, float &x, float &y) {
        if (!j || j->t != J::Arr || j->a.size() < 2) return false;
        x = clampf((float)j->a[0].num(x), kMinPos, kMaxPos);
        y = clampf((float)j->a[1].num(y), kMinPos, kMaxPos);
        return true;
    };

    if (const J *cs = root.get("clusters"); cs && cs->t == J::Obj) {
        for (auto &kv : l.clusters) {
            const J *c = cs->get(kv.first.c_str());
            if (!c || c->t != J::Obj) continue;      /* newer cluster: default */
            point(c->get("pos"), kv.second.dx, kv.second.dy);
            if (const J *s = c->get("scale"))
                kv.second.scale = clampf((float)s->num(1.0), kMinScale, kMaxScale);
            /* Per-cluster spacing was the Layout-level field until
             * 2026-09-18; older JSONs still write it at the top level,
             * which we ignore here (the per-cluster default 1.0 wins). */
            (void)c->get("spacing");
            if (const J *sp = c->get("spacing"))
                kv.second.spacing = clampf((float)sp->num(1.0), 0.5f, 2.5f);
            if (const J *v = c->get("visible")) kv.second.visible = v->boolean(true);
        }
    }
    if (const J *ex = root.get("extras"); ex && ex->t == J::Arr) {
        for (const J &e : ex->a) {
            const J *act = e.get("action");
            if (!act) continue;
            Extra x;
            x.id = act->str("");
            if (const J *id = act->get("id")) x.id = id->str("");
            if (x.id.empty()) continue;
            x.label = x.id;
            if (const J *lb = act->get("label")) x.label = lb->str(x.id);
            if (x.is_direction()) x.label = x.id.substr(4);
            for (char &ch : x.label) if (x.is_direction()) ch = (char)toupper((unsigned char)ch);
            x.dx = 0.5f; x.dy = 0.5f;
            if (!point(e.get("pos"), x.dx, x.dy)) continue;
            if (const J *s = e.get("scale")) x.scale = clampf((float)s->num(1.0), kMinScale, kMaxScale);
            x.spacing = 1.0f;
            if (const J *sp = e.get("spacing")) x.spacing = clampf((float)sp->num(1.0), 0.5f, 2.5f);
            bool dup = false;
            for (const Extra &o : l.extras) if (o.id == x.id) dup = true;
            if (!dup) l.extras.push_back(x);
        }
    }
    return l;
}

std::string Layout::encode() const
{
    std::string o = "{\"version\":1,\"profile\":" + quote(profile) +
                    ",\"stick\":" + quote(stick == Stick::Dpad ? "dpad" : "wobble") +
                    ",\"opacity\":" + num(opacity) + ",\"clusters\":{";
    bool first = true;
    for (const auto &kv : clusters) {
        if (!first) o += ",";
        first = false;
        o += quote(kv.first) + ":{\"pos\":[" + num(kv.second.dx) + "," + num(kv.second.dy) +
             "],\"scale\":" + num(kv.second.scale) +
             ",\"spacing\":" + num(kv.second.spacing) +
             ",\"visible\":" + (kv.second.visible ? "true" : "false") + "}";
    }
    o += "},\"extras\":[";
    first = true;
    for (const Extra &e : extras) {
        if (!first) o += ",";
        first = false;
        o += "{\"action\":{\"id\":" + quote(e.id) + ",\"label\":" + quote(e.label);
        if (e.is_direction())
            o += ",\"kind\":\"direction\",\"code\":0,\"direction\":" + quote(e.id.substr(4));
        else
            o += ",\"kind\":\"button\",\"code\":0";
        o += "},\"pos\":[" + num(e.dx) + "," + num(e.dy) + "],\"scale\":" + num(e.scale) + "}";
    }
    return o + "]}";
}

std::string Layout::file_for(const std::string &dir, const Profile &p)
{
    std::string d = dir;
    if (!d.empty() && d.back() != '/' && d.back() != '\\') d += '/';
    return d + "pad_layout_" + p.id + ".json";
}

Layout Layout::load(const std::string &dir, const Profile &p)
{
    std::ifstream f(file_for(dir, p), std::ios::binary);
    if (!f) return defaults(p);
    std::stringstream ss; ss << f.rdbuf();
    return decode(ss.str(), p);
}

bool Layout::save(const std::string &dir) const
{
    const Profile *p = profile_by_id(profile);
    if (!p) return false;
    const std::string path = file_for(dir, *p);
    const std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f << encode();
        if (!f) return false;
    }
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

/* ==================================================================== */
/* Overlay                                                               */
/* ==================================================================== */

namespace {

const SDL_FingerID kMouseFinger = (SDL_FingerID)-7;
/* Room for a cluster's name above it while arranging. A constant, not the
 * font size: hits are tested between frames. */
const float kLabelStrip = 18.0f;

ImU32 with_alpha(ImU32 c, float a)
{
    return (c & ~IM_COL32_A_MASK) |
           ((ImU32)(int)(255.0f * clampf(a, 0.0f, 1.0f)) << IM_COL32_A_SHIFT);
}

/* An estimate, not a measurement: events are handled between frames, when
 * ImGui has no baked font to ask, and hit boxes must match what is drawn
 * whichever side computes them. Bold capitals in the UI font run at about
 * 0.62 em, and a pill is padded anyway. */
float text_width(const char *s, float font_size)
{
    return (float)strlen(s) * font_size * 0.62f;
}

} /* namespace */

void Overlay::set(const Profile *p, const Layout &l)
{
    prof_ = p; lay_ = l;
    pointers_.clear(); held_.clear(); mouse_down_ = false;
    stick_knob_ = ImVec2(0, 0); stick_active_ = false;
    selected_.clear(); dragging_.clear();
    hits_.clear();
}

void Overlay::set_editing(bool on)
{
    if (editing_ == on) return;
    editing_ = on;
    pointers_.clear(); mouse_down_ = false; dragging_.clear();
    if (!on) selected_.clear();
}

void Overlay::release_all(const Sink &sink)
{
    for (auto &kv : held_)
        if (kv.second > 0 && kv.first.rfind("dir:", 0) != 0 && sink.action)
            sink.action(kv.first, false);
    held_.clear();
    pointers_.clear(); mouse_down_ = false;
    stick_knob_ = ImVec2(0, 0); stick_active_ = false;
    recompute(sink);
}

/*
 * Every pressable thing, in pixels, for this frame's area. Rebuilt each draw
 * (and on demand by handle) because the window can change size under a
 * finger.
 */
void Overlay::rebuild_hits(const ImVec2 &pos, const ImVec2 &size)
{
    hits_.clear();
    last_pos_ = pos; last_size_ = size;
    if (!prof_) return;

    auto centre_of = [&](float dx, float dy) {
        return ImVec2(pos.x + dx * size.x, pos.y + dy * size.y);
    };
    /* Push each cluster outward from the play-area centre by `spacing`.
     * 1.0 keeps the profile's chosen positions; >1 spreads (useful when a
     * cluster has been scaled up past its slot), <1 brings everything
     * towards the middle. The extras use the same math so an added
     * button follows the global spread. */
    auto add_circle = [&](const char *kind, const std::string &id, ImVec2 c, float r) {
        hits_.push_back(Hit{ kind, id, c, r, ImVec2(0, 0) });
    };
    auto add_box = [&](const char *kind, const std::string &id, ImVec2 c, ImVec2 half) {
        hits_.push_back(Hit{ kind, id, c, 0.0f, half });
    };

    for (const Cluster &c : prof_->clusters) {
        auto it = lay_.clusters.find(c.id);
        if (it == lay_.clusters.end()) continue;
        const Placed &p = it->second;
        if (!p.visible && !editing_) continue;
        const ImVec2 cc = centre_of(p.dx, p.dy);
        const float sc = p.scale;
        if (c.is_stick) {
            add_circle("stick", c.id, cc, c.stick_size * sc * 0.5f);
            continue;
        }
        /* Per-cluster spacing multiplies the gap between buttons INSIDE
         * this cluster. 1.0 is the default; >1 spreads them, <1
         * packs them. Combined with the cluster's own scale (sc),
         * this lets the user push buttons apart without disturbing
         * the cluster's position in the play area. */
        const float sp = p.spacing;
        const float gap = 10.0f * sc * sp;
        switch (c.shape) {
        case Shape::Column: {
            float total = 0;
            for (const Button &b : c.buttons) total += b.size * sc;
            total += gap * (float)(c.buttons.size() - 1);
            float y = cc.y - total * 0.5f;
            for (const Button &b : c.buttons) {
                const float s = b.size * sc;
                add_circle("button", b.id, ImVec2(cc.x, y + s * 0.5f), s * 0.5f);
                y += s + gap;
            }
            break;
        }
        case Shape::Row: {
            std::vector<float> widths;
            float total = 0;
            for (const Button &b : c.buttons) {
                const float s = b.size * sc;
                const float w = b.face == Face::Pill
                    ? std::max(s, text_width(b.label.c_str(), s * 0.34f) + s * 0.6f) : s;
                widths.push_back(w); total += w;
            }
            total += gap * (float)(c.buttons.size() - 1);
            float x = cc.x - total * 0.5f;
            for (size_t i = 0; i < c.buttons.size(); ++i) {
                const float s = c.buttons[i].size * sc;
                if (c.buttons[i].face == Face::Pill)
                    add_box("button", c.buttons[i].id, ImVec2(x + widths[i] * 0.5f, cc.y),
                            ImVec2(widths[i] * 0.5f, s * 0.5f));
                else
                    add_circle("button", c.buttons[i].id, ImVec2(x + widths[i] * 0.5f, cc.y), s * 0.5f);
                x += widths[i] + gap;
            }
            break;
        }
        case Shape::Diamond: {
            float d = 0;
            for (const Button &b : c.buttons) d = std::max(d, b.size * sc);
            const float side = d * 2.7f * sp;
            const ImVec2 at[4] = { ImVec2(cc.x, cc.y - side * 0.5f + d * 0.5f),
                                   ImVec2(cc.x - side * 0.5f + d * 0.5f, cc.y),
                                   ImVec2(cc.x + side * 0.5f - d * 0.5f, cc.y),
                                   ImVec2(cc.x, cc.y + side * 0.5f - d * 0.5f) };
            for (size_t i = 0; i < c.buttons.size() && i < 4; ++i)
                add_circle("button", c.buttons[i].id, at[i], c.buttons[i].size * sc * 0.5f);
            break;
        }
        case Shape::Grid3x2: {
            for (size_t i = 0; i < c.buttons.size() && i < 6; ++i) {
                const int row = (int)(i / 3), col = (int)(i % 3);
                float rowh = 0, roww = 0;
                for (size_t j = row * 3; j < (size_t)row * 3 + 3 && j < c.buttons.size(); ++j) {
                    rowh = std::max(rowh, c.buttons[j].size * sc);
                    roww += c.buttons[j].size * sc + (j > (size_t)row * 3 ? gap : 0);
                }
                float toph = 0, both = 0;
                for (size_t j = 0; j < 3 && j < c.buttons.size(); ++j) toph = std::max(toph, c.buttons[j].size * sc);
                for (size_t j = 3; j < 6 && j < c.buttons.size(); ++j) both = std::max(both, c.buttons[j].size * sc);
                const float total_h = toph + gap + both;
                const float y = cc.y - total_h * 0.5f + (row == 0 ? toph * 0.5f : toph + gap + both * 0.5f);
                float x = cc.x - roww * 0.5f;
                for (int k = 0; k < col; ++k) x += c.buttons[(size_t)row * 3 + (size_t)k].size * sc + gap;
                x += c.buttons[i].size * sc * 0.5f;
                add_circle("button", c.buttons[i].id, ImVec2(x, y), c.buttons[i].size * sc * 0.5f);
                (void)rowh;
            }
            break;
        }
        }
    }
    for (const Extra &e : lay_.extras) {
        const float s = 52.0f * e.scale;
        const float w = std::max(s, text_width(e.label.c_str(), s * 0.3f) + s * 0.6f);
        add_box("extra", e.id, centre_of(e.dx, e.dy), ImVec2(w * 0.5f, s * 0.5f));
    }
}

const Overlay::Hit *Overlay::hit_at(const ImVec2 &p) const
{
    /* Extras were added last and sit on top. */
    for (auto it = hits_.rbegin(); it != hits_.rend(); ++it) {
        const Hit &h = *it;
        if (h.radius > 0) {
            const float dx = p.x - h.centre.x, dy = p.y - h.centre.y;
            /* A little slack round a button: a thumb is not a point. */
            const float r = h.radius * (h.kind == "stick" ? 1.0f : 1.15f);
            if (dx * dx + dy * dy <= r * r) return &h;
        } else {
            if (std::fabs(p.x - h.centre.x) <= h.half.x * 1.1f + 4 &&
                std::fabs(p.y - h.centre.y) <= h.half.y * 1.1f + 4) return &h;
        }
    }
    return nullptr;
}

/* Editing: which movable thing is under a point. Extras by themselves;
 * a cluster by the whole framed box drawn round it, not just its buttons --
 * the middle of a diamond is empty, and a grab that misses there reads as
 * a pad that will not move. */
std::string Overlay::select_at(const ImVec2 &p) const
{
    if (!prof_) return "";
    /* Extras first: they can sit over a cluster. */
    for (const Extra &e : lay_.extras) {
        const ImVec2 c(last_pos_.x + e.dx * last_size_.x, last_pos_.y + e.dy * last_size_.y);
        const float s = 52.0f * e.scale;
        const float w = std::max(s, text_width(e.label.c_str(), s * 0.3f) + s * 0.6f);
        if (std::fabs(p.x - c.x) <= w * 0.5f + 12 && std::fabs(p.y - c.y) <= s * 0.5f + 16)
            return "extra:" + e.id;
    }
    for (const Cluster &c : prof_->clusters) {
        ImVec2 mn(FLT_MAX, FLT_MAX), mx(-FLT_MAX, -FLT_MAX);
        bool any = false;
        for (const Hit &h : hits_) {
            bool mine = (h.kind == "stick" && h.id == c.id);
            if (!mine && h.kind == "button")
                for (const Button &b : c.buttons) if (b.id == h.id) mine = true;
            if (!mine) continue;
            any = true;
            const float hx = h.radius > 0 ? h.radius : h.half.x;
            const float hy = h.radius > 0 ? h.radius : h.half.y;
            mn.x = std::min(mn.x, h.centre.x - hx); mn.y = std::min(mn.y, h.centre.y - hy);
            mx.x = std::max(mx.x, h.centre.x + hx); mx.y = std::max(mx.y, h.centre.y + hy);
        }
        if (!any) continue;
        /* The same padding the frame is drawn with, label strip included. */
        if (p.x >= mn.x - 10 && p.x <= mx.x + 10 &&
            p.y >= mn.y - 10 - kLabelStrip && p.y <= mx.y + 10)
            return c.id;
    }
    return "";
}

void Overlay::press(const std::string &id, bool down, const Sink &sink)
{
    int &n = held_[id];
    const bool was = n > 0;
    n = std::max(0, n + (down ? 1 : -1));
    const bool now = n > 0;
    if (was == now) return;
    if (id.rfind("dir:", 0) == 0) { recompute(sink); return; }
    if (sink.action) sink.action(id, now);
}

/* The stick's four bits and the direction buttons, merged, sent on change. */
void Overlay::recompute(const Sink &sink)
{
    bool u = false, d = false, l = false, r = false;
    for (const auto &kv : pointers_) {
        if (!kv.second.stick) continue;
        const Hit *h = nullptr;
        for (const Hit &x : hits_) if (x.kind == "stick") h = &x;
        if (!h) continue;
        const float dx = (kv.second.pos.x - h->centre.x) / h->radius;
        const float dy = (kv.second.pos.y - h->centre.y) / h->radius;
        const float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < 0.18f) continue;
        if (lay_.stick == Stick::Wobble) {
            /* Eight 45-degree sectors centred on the compass points. */
            float deg = std::atan2(dy, dx) * 180.0f / 3.14159265f;
            if (deg < 0) deg += 360.0f;
            const int sector = (int)std::floor((deg + 22.5f) / 45.0f) % 8;
            /* 0 R, 1 DR, 2 D, 3 DL, 4 L, 5 UL, 6 U, 7 UR */
            r |= sector == 0 || sector == 1 || sector == 7;
            d |= sector == 1 || sector == 2 || sector == 3;
            l |= sector == 3 || sector == 4 || sector == 5;
            u |= sector == 5 || sector == 6 || sector == 7;
        } else {
            /* Lower than 45 degrees on purpose: wider diagonal corners, which
             * are hard to hold on glass with no edge to find. */
            const float t = 0.38f;
            if (dx <= -t) l = true;
            if (dx >= t)  r = true;
            if (dy <= -t) u = true;
            if (dy >= t)  d = true;
        }
    }
    auto held = [&](const char *id) { auto it = held_.find(id); return it != held_.end() && it->second > 0; };
    u |= held("dir:up"); d |= held("dir:down"); l |= held("dir:left"); r |= held("dir:right");
    stick_active_ = u || d || l || r;
    if (u == sent_[0] && d == sent_[1] && l == sent_[2] && r == sent_[3]) return;
    sent_[0] = u; sent_[1] = d; sent_[2] = l; sent_[3] = r;
    if (sink.directions) sink.directions(u, d, l, r);
}

bool Overlay::handle(const SDL_Event &ev, const ImVec2 &window,
                     const ImVec2 &area_pos, const ImVec2 &area_size, const Sink &sink)
{
    if (!prof_) return false;
    if (hits_.empty() || last_size_.x != area_size.x || last_size_.y != area_size.y)
        rebuild_hits(area_pos, area_size);

    SDL_FingerID id; ImVec2 p; int phase;   /* 0 down, 1 move, 2 up */
    switch (ev.type) {
    case SDL_EVENT_FINGER_DOWN:   phase = 0; goto finger;
    case SDL_EVENT_FINGER_MOTION: phase = 1; goto finger;
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED: phase = 2;
    finger:
        id = ev.tfinger.fingerID;
        /* Fingers arrive normalised to the window, whatever the area is. */
        p = ImVec2(ev.tfinger.x * window.x, ev.tfinger.y * window.y);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (ev.button.button != SDL_BUTTON_LEFT || ev.button.which == SDL_TOUCH_MOUSEID) return false;
        id = kMouseFinger; p = ImVec2(ev.button.x, ev.button.y); phase = 0; mouse_down_ = true;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (!mouse_down_ || ev.motion.which == SDL_TOUCH_MOUSEID) return false;
        id = kMouseFinger; p = ImVec2(ev.motion.x, ev.motion.y); phase = 1;
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (ev.button.button != SDL_BUTTON_LEFT || ev.button.which == SDL_TOUCH_MOUSEID) return false;
        if (!mouse_down_) return false;
        id = kMouseFinger; p = ImVec2(ev.button.x, ev.button.y); phase = 2; mouse_down_ = false;
        break;
    default:
        return false;
    }

    /* ---- arranging ---- */
    if (editing_) {
        if (phase == 0) {
            /* A tap on an extra's red badge removes it. */
            for (const Extra &e : lay_.extras) {
                const ImVec2 c(area_pos.x + e.dx * area_size.x, area_pos.y + e.dy * area_size.y);
                const float s = 52.0f * e.scale;
                const float w = std::max(s, text_width(e.label.c_str(), s * 0.3f) + s * 0.6f);
                const ImVec2 badge(c.x + w * 0.5f + 6, c.y - s * 0.5f - 6);
                if (std::fabs(p.x - badge.x) <= 16 && std::fabs(p.y - badge.y) <= 16) {
                    const std::string gone = e.id;
                    lay_.extras.erase(std::remove_if(lay_.extras.begin(), lay_.extras.end(),
                        [&](const Extra &x) { return x.id == gone; }), lay_.extras.end());
                    if (selected_ == "extra:" + gone) selected_.clear();
                    dirty_ = true;
                    rebuild_hits(area_pos, area_size);
                    return true;
                }
            }
            const std::string s = select_at(p);
            if (!s.empty()) { selected_ = s; dragging_ = s; drag_last_ = p; }
            return true;
        }
        if (phase == 1 && !dragging_.empty()) {
            const float fx = (p.x - drag_last_.x) / area_size.x;
            const float fy = (p.y - drag_last_.y) / area_size.y;
            drag_last_ = p;
            if (dragging_.rfind("extra:", 0) == 0) {
                for (Extra &e : lay_.extras)
                    if ("extra:" + e.id == dragging_) {
                        e.dx = clampf(e.dx + fx, kMinPos, kMaxPos);
                        e.dy = clampf(e.dy + fy, kMinPos, kMaxPos);
                    }
            } else {
                auto it = lay_.clusters.find(dragging_);
                if (it != lay_.clusters.end()) {
                    it->second.dx = clampf(it->second.dx + fx, kMinPos, kMaxPos);
                    it->second.dy = clampf(it->second.dy + fy, kMinPos, kMaxPos);
                }
            }
            rebuild_hits(area_pos, area_size);
            return true;
        }
        if (phase == 2) {
            if (!dragging_.empty()) { dragging_.clear(); dirty_ = true; }
            return true;
        }
        return true;
    }

    /* ---- playing ---- */
    if (phase == 0) {
        const Hit *h = hit_at(p);
        if (!h) return false;
        Pointer ptr; ptr.pos = p;
        if (h->kind == "stick") {
            ptr.stick = true;
            pointers_[id] = ptr;
            recompute(sink);
        } else {
            ptr.on = h->id;
            pointers_[id] = ptr;
            press(h->id, true, sink);
        }
        return true;
    }
    auto it = pointers_.find(id);
    if (it == pointers_.end()) return false;
    Pointer &ptr = it->second;
    ptr.pos = p;
    if (phase == 1) {
        if (ptr.stick) {
            recompute(sink);
        } else {
            /* Sliding from one button onto the next changes the press
             * without lifting off, the way a thumb rolls across a pad. */
            const Hit *h = hit_at(p);
            const std::string now = (h && h->kind != "stick") ? h->id : ptr.on;
            if (now != ptr.on) {
                press(ptr.on, false, sink);
                ptr.on = now;
                press(now, true, sink);
            }
        }
        return true;
    }
    /* up */
    if (ptr.stick) {
        pointers_.erase(it);
        recompute(sink);
    } else {
        const std::string on = ptr.on;
        pointers_.erase(it);
        press(on, false, sink);
    }
    return true;
}

/* -------------------------------------------------------------------- */
/* Drawing                                                              */
/* -------------------------------------------------------------------- */

void Overlay::draw(ImDrawList *dl, const ImVec2 &area_pos, const ImVec2 &area_size)
{
    if (!prof_) return;
    rebuild_hits(area_pos, area_size);
    const float op = editing_ ? 1.0f : lay_.opacity;
    ImFont *font = ImGui::GetFont();

    /* Where the stick pointer is, for the knob. */
    ImVec2 knob(0, 0);
    bool have_knob = false;
    for (const auto &kv : pointers_)
        if (kv.second.stick) { knob = kv.second.pos; have_knob = true; }

    auto label = [&](ImVec2 c, const char *text, float size, ImU32 col) {
        const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
        dl->AddText(font, size, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), col, text);
    };

    for (const Hit &h : hits_) {
        if (h.kind == "stick") {
            const Placed &p = lay_.clusters[h.id];
            const float dim = p.visible ? 1.0f : 0.3f;
            const float r = h.radius;
            const ImVec2 c = h.centre;
            if (lay_.stick == Stick::Wobble) {
                dl->AddCircleFilled(c, r, with_alpha(IM_COL32(0x5F, 0x66, 0x70, 255), 0.27f * op * dim), 48);
                dl->AddCircle(c, r - 2, with_alpha(IM_COL32(0xD6, 0xDA, 0xDF, 255), 0.6f * op * dim), 48, std::max(2.0f, r * 0.04f));
                dl->AddCircle(c, r * 0.35f, with_alpha(IM_COL32(0xD6, 0xDA, 0xDF, 255), 0.2f * op * dim), 32, 1.0f);
                dl->AddCircle(c, r * 0.68f, with_alpha(IM_COL32(0xD6, 0xDA, 0xDF, 255), 0.2f * op * dim), 32, 1.0f);
                ImVec2 off(0, 0);
                if (have_knob) {
                    off = ImVec2(knob.x - c.x, knob.y - c.y);
                    const float d = std::sqrt(off.x * off.x + off.y * off.y);
                    const float maxd = r * 0.6f;
                    if (d > maxd) { off.x *= maxd / d; off.y *= maxd / d; }
                }
                const ImVec2 kc(c.x + off.x, c.y + off.y);
                const float kr = r * 0.4f;
                dl->AddCircleFilled(ImVec2(kc.x - off.x * 0.15f, kc.y - off.y * 0.15f + 3), kr * 0.95f,
                                    with_alpha(IM_COL32(0, 0, 0, 255), 0.35f * op * dim), 32);
                dl->AddCircleFilled(kc, kr, with_alpha(stick_active_ ? kAccent : IM_COL32(0xC5, 0xCB, 0xD3, 255), 0.88f * op * dim), 32);
                dl->AddCircleFilled(ImVec2(kc.x - kr * 0.3f, kc.y - kr * 0.3f), kr * 0.35f,
                                    with_alpha(IM_COL32(255, 255, 255, 255), 0.45f * op * dim), 16);
                dl->AddCircle(kc, kr, with_alpha(IM_COL32(255, 255, 255, 255), 0.93f * op * dim), 32, std::max(1.5f, r * 0.024f));
            } else {
                const float arm = r * 2.0f * 0.30f;
                const ImU32 fill = with_alpha(IM_COL32(255, 255, 255, 255), 0.10f * op * dim);
                const ImU32 on   = with_alpha(kAccent, 0.55f * op * dim);
                const ImU32 line = with_alpha(IM_COL32(255, 255, 255, 255), 0.55f * op * dim);
                const float rnd = arm * 0.25f;
                dl->AddRectFilled(ImVec2(c.x - arm / 2, c.y - r), ImVec2(c.x + arm / 2, c.y + r), fill, rnd);
                dl->AddRectFilled(ImVec2(c.x - r, c.y - arm / 2), ImVec2(c.x + r, c.y + arm / 2), fill, rnd);
                if (sent_[0]) dl->AddRectFilled(ImVec2(c.x - arm / 2, c.y - r), ImVec2(c.x + arm / 2, c.y - arm / 2), on, rnd);
                if (sent_[1]) dl->AddRectFilled(ImVec2(c.x - arm / 2, c.y + arm / 2), ImVec2(c.x + arm / 2, c.y + r), on, rnd);
                if (sent_[2]) dl->AddRectFilled(ImVec2(c.x - r, c.y - arm / 2), ImVec2(c.x - arm / 2, c.y + arm / 2), on, rnd);
                if (sent_[3]) dl->AddRectFilled(ImVec2(c.x + arm / 2, c.y - arm / 2), ImVec2(c.x + r, c.y + arm / 2), on, rnd);
                dl->AddRect(ImVec2(c.x - arm / 2, c.y - r), ImVec2(c.x + arm / 2, c.y + r), line, rnd, 0, 1.5f);
                dl->AddRect(ImVec2(c.x - r, c.y - arm / 2), ImVec2(c.x + r, c.y + arm / 2), line, rnd, 0, 1.5f);
                const float g = arm * 0.28f;
                const ImU32 glyph = with_alpha(IM_COL32(255, 255, 255, 255), 0.7f * op * dim);
                dl->AddTriangleFilled(ImVec2(c.x, c.y - r + g * 0.8f), ImVec2(c.x - g, c.y - r + g * 2), ImVec2(c.x + g, c.y - r + g * 2), glyph);
                dl->AddTriangleFilled(ImVec2(c.x, c.y + r - g * 0.8f), ImVec2(c.x + g, c.y + r - g * 2), ImVec2(c.x - g, c.y + r - g * 2), glyph);
                dl->AddTriangleFilled(ImVec2(c.x - r + g * 0.8f, c.y), ImVec2(c.x - r + g * 2, c.y + g), ImVec2(c.x - r + g * 2, c.y - g), glyph);
                dl->AddTriangleFilled(ImVec2(c.x + r - g * 0.8f, c.y), ImVec2(c.x + r - g * 2, c.y - g), ImVec2(c.x + r - g * 2, c.y + g), glyph);
            }
            continue;
        }

        /* A button, from a cluster or an extra. */
        const Button *spec = nullptr;
        const Cluster *owner = nullptr;
        for (const Cluster &c : prof_->clusters)
            for (const Button &b : c.buttons)
                if (b.id == h.id && h.kind == "button") { spec = &b; owner = &c; }
        const Extra *extra = nullptr;
        if (h.kind == "extra")
            for (const Extra &e : lay_.extras) if (e.id == h.id) extra = &e;
        if (!spec && !extra) continue;

        const bool down = held_.count(h.id) && held_[h.id] > 0;
        float dim = 1.0f;
        if (owner) { const Placed &p = lay_.clusters[owner->id]; if (!p.visible) dim = 0.3f; }
        const ImU32 base = spec ? spec->colour
                         : (extra->is_direction() ? kAccent : IM_COL32(0x8A, 0x94, 0xA6, 255));
        const ImU32 fill = with_alpha(base, (down ? 0.9f : 0.45f) * op * dim);
        const ImU32 edge = with_alpha(IM_COL32(255, 255, 255, 255), (down ? 0.95f : 0.65f) * op * dim);
        const ImU32 ink  = with_alpha(IM_COL32(255, 255, 255, 255), op * dim);
        const float s = h.radius > 0 ? h.radius * 2 : h.half.y * 2;
        const float fs = (spec ? spec->label : extra->label).size() > 3 ? s * 0.26f : s * (s < 50 ? 0.36f : 0.32f);
        const char *text = spec ? spec->label.c_str() : extra->label.c_str();
        if (h.radius > 0 && (!spec || spec->face == Face::Circle)) {
            dl->AddCircleFilled(h.centre, h.radius, fill, 40);
            dl->AddCircle(h.centre, h.radius, edge, 40, 2.0f);
        } else {
            const ImVec2 a(h.centre.x - h.half.x, h.centre.y - h.half.y);
            const ImVec2 b(h.centre.x + h.half.x, h.centre.y + h.half.y);
            const float rnd = (spec && spec->face == Face::Square) ? h.half.y * 0.36f : h.half.y;
            dl->AddRectFilled(a, b, fill, rnd);
            dl->AddRect(a, b, edge, rnd, 0, 2.0f);
        }
        label(h.centre, text, fs, ink);
    }

    /* ---- arranging chrome ---- */
    if (!editing_) return;
    auto frame = [&](ImVec2 a, ImVec2 b, const char *text, bool sel) {
        const ImU32 col = sel ? kAccent : IM_COL32(255, 255, 255, 110);
        dl->AddRectFilled(a, b, IM_COL32(0, 0, 0, 90), 12);
        dl->AddRect(a, b, col, 12, 0, 2.0f);
        if (text && *text) {
            const float fs = ImGui::GetFontSize() * 0.8f;
            dl->AddText(font, fs, ImVec2(a.x + 8, a.y + 4), col, text);
        }
    };
    for (const Cluster &c : prof_->clusters) {
        ImVec2 mn(FLT_MAX, FLT_MAX), mx(-FLT_MAX, -FLT_MAX);
        bool any = false;
        for (const Hit &h : hits_) {
            bool mine = (h.kind == "stick" && h.id == c.id);
            if (!mine && h.kind == "button")
                for (const Button &b : c.buttons) if (b.id == h.id) mine = true;
            if (!mine) continue;
            any = true;
            const float hx = h.radius > 0 ? h.radius : h.half.x, hy = h.radius > 0 ? h.radius : h.half.y;
            mn.x = std::min(mn.x, h.centre.x - hx); mn.y = std::min(mn.y, h.centre.y - hy);
            mx.x = std::max(mx.x, h.centre.x + hx); mx.y = std::max(mx.y, h.centre.y + hy);
        }
        if (!any) continue;
        frame(ImVec2(mn.x - 10, mn.y - 10 - kLabelStrip), ImVec2(mx.x + 10, mx.y + 10),
              c.label.c_str(), selected_ == c.id);
    }
    for (const Extra &e : lay_.extras) {
        for (const Hit &h : hits_) {
            if (h.kind != "extra" || h.id != e.id) continue;
            const ImVec2 a(h.centre.x - h.half.x - 8, h.centre.y - h.half.y - 8);
            const ImVec2 b(h.centre.x + h.half.x + 8, h.centre.y + h.half.y + 8);
            frame(a, b, "", selected_ == "extra:" + e.id);
            /* The red badge, inside room made for it. */
            const ImVec2 badge(h.centre.x + h.half.x + 6, h.centre.y - h.half.y - 6);
            dl->AddCircleFilled(badge, 11, IM_COL32(255, 82, 82, 255), 20);
            dl->AddLine(ImVec2(badge.x - 4, badge.y - 4), ImVec2(badge.x + 4, badge.y + 4), IM_COL32_WHITE, 2);
            dl->AddLine(ImVec2(badge.x - 4, badge.y + 4), ImVec2(badge.x + 4, badge.y - 4), IM_COL32_WHITE, 2);
        }
    }
}

/* -------------------------------------------------------------------- */
/* The designer's controls (ImGui widgets, drawn by the host wherever)   */
/* -------------------------------------------------------------------- */

bool Overlay::designer_controls(const Sink *sink_for_release)
{
    if (!prof_) return false;
    bool changed = false;

    ImGui::TextUnformatted("Movement");
    ImGui::SameLine();
    int st = lay_.stick == Stick::Dpad ? 1 : 0;
    if (ImGui::RadioButton("Stick", &st, 0)) changed = true;
    ImGui::SameLine();
    if (ImGui::RadioButton("D-pad", &st, 1)) changed = true;
    if (changed) { lay_.stick = st ? Stick::Dpad : Stick::Wobble; if (sink_for_release) release_all(*sink_for_release); }

    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 9.0f);
    float op = lay_.opacity * 100.0f;
    if (ImGui::SliderFloat("Opacity", &op, 20.0f, 100.0f, "%.0f%%")) {
        lay_.opacity = op / 100.0f; changed = true;
    }

    /* Spacing used to be a global Layout-level field; 2026-09-18 moved
     * it to per-cluster Placed::spacing so the slider sits with the
     * rest of the cluster panel (cluster_panel()). Nothing left to
     * draw here. */

    /* Add a button: a direction (where the stick is a joystick) or a second
     * copy of any of the pad's own buttons, placed independently. The act
     * of adding sets selected_ to the new extra, which is why this lives
     * here rather than under the cluster panel -- a freshly-added button
     * should bring the popup up automatically, which the host sees because
     * selected_ went non-empty. */
    ImGui::Spacing();
    if (ImGui::BeginCombo("##addbtn", "Add a button...")) {
        auto have = [&](const std::string &id) {
            for (const Extra &e : lay_.extras) if (e.id == id) return true;
            return false;
        };
        auto add = [&](const std::string &id, const std::string &lbl) {
            const int n = (int)lay_.extras.size();
            Extra e; e.id = id; e.label = lbl;
            e.dx = clampf(0.30f + 0.10f * (float)(n % 5), kMinPos, kMaxPos);
            e.dy = clampf(0.12f + 0.14f * (float)(n / 5), kMinPos, kMaxPos);
            lay_.extras.push_back(e);
            selected_ = "extra:" + id;
            changed = true;
        };
        if (prof_->allow_direction_buttons) {
            ImGui::TextDisabled("Joystick direction");
            const char *dirs[4][2] = { {"dir:up", "UP"}, {"dir:down", "DOWN"}, {"dir:left", "LEFT"}, {"dir:right", "RIGHT"} };
            for (auto &d : dirs) {
                ImGui::BeginDisabled(have(d[0]));
                if (ImGui::Selectable(d[1])) add(d[0], d[1]);
                ImGui::EndDisabled();
            }
        }
        ImGui::TextDisabled("Buttons");
        for (const Cluster &c : prof_->clusters)
            for (const Button &b : c.buttons) {
                ImGui::BeginDisabled(have(b.id));
                if (ImGui::Selectable(b.label.c_str())) add(b.id, b.label);
                ImGui::EndDisabled();
            }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset layout")) {
        lay_ = Layout::defaults(*prof_);
        selected_.clear();
        changed = true;
    }
    if (changed) hits_.clear();
    return changed;
}

/* Cluster-specific panel: Size, Shown, Remove. The host opens a popup
 * while something is selected; this method draws the body. Returns true
 * when the layout changed and wants saving. */
bool Overlay::cluster_panel()
{
    if (!prof_ || selected_.empty()) return false;
    bool changed = false;
    float *scale = nullptr; bool *visible = nullptr; float *spacing = nullptr;
    std::string title;
    if (selected_.rfind("extra:", 0) == 0) {
        for (Extra &e : lay_.extras)
            if ("extra:" + e.id == selected_) {
                scale = &e.scale; spacing = &e.spacing; title = e.label;
            }
    } else if (auto it = lay_.clusters.find(selected_); it != lay_.clusters.end()) {
        scale = &it->second.scale; visible = &it->second.visible;
        spacing = &it->second.spacing;
        if (const Cluster *c = prof_->cluster(selected_)) title = c->label;
    }
    if (!scale) { selected_.clear(); return false; }

    ImGui::Text("%s", title.c_str());
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12.0f);
    if (ImGui::SliderFloat("Size", scale, kMinScale, kMaxScale, "%.2fx")) changed = true;
    if (visible) {
        ImGui::SameLine();
        if (ImGui::Checkbox("Shown", visible)) changed = true;
    }
    /* Spacing is per-cluster (Placed::spacing), not global. Lives here
     * because adjusting it only makes sense in the context of the
     * selected cluster -- the one you've picked out and are
     * respacing. The slider in designer_controls() used to be global
     * and was the wrong knob. */
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12.0f);
    if (ImGui::SliderFloat("Spacing", spacing, 0.5f, 2.5f, "%.2fx")) changed = true;
    if (selected_.rfind("extra:", 0) == 0) {
        if (ImGui::Button("Remove")) {
            const std::string gone = selected_.substr(6);
            lay_.extras.erase(std::remove_if(lay_.extras.begin(), lay_.extras.end(),
                [&](const Extra &x) { return x.id == gone; }), lay_.extras.end());
            selected_.clear();
            ImGui::CloseCurrentPopup();
            changed = true;
        }
        ImGui::SameLine();
    }
    /* Done used to just clear selected_, which left the modal stuck
     * open on the popup stack -- the user reported a grey button and no
     * way out. CloseCurrentPopup() actually dismisses it; selected_
     * also gets cleared so has_selection() goes false and we don't
     * re-open next frame. ImGui's modal popups already bind Escape. */
    if (ImGui::Button("Done")) {
        selected_.clear(); ImGui::CloseCurrentPopup();
    }
    if (changed) hits_.clear();
    return changed;
}

} /* namespace touchpad */
