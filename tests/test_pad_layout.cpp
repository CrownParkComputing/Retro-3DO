// Default on-screen control layout.
//
// Both assertions here are regressions for bugs that were invisible in the code
// and obvious the moment the layout was rendered: a d-pad stretched into a wide
// diamond, and clusters placed so near the edge that their outermost controls
// hung off the screen and could never be pressed.
#include <cmath>

#include "app/pad_layout.h"
#include "test_harness.h"

using namespace retro3do;

namespace {

const TouchControl* find(const std::vector<TouchControl>& layout, retro3do_pad_button button) {
    for (const TouchControl& control : layout) {
        if (control.button == button) return &control;
    }
    return nullptr;
}

// Distance in pixels between two controls on a screen of this size.
float pixel_distance(const TouchControl& a, const TouchControl& b, int w, int h) {
    const float dx = (a.x - b.x) * static_cast<float>(w);
    const float dy = (a.y - b.y) * static_cast<float>(h);
    return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

TEST(every_control_is_fully_on_screen) {
    // A control whose edge falls outside the screen cannot be pressed, and in
    // layout mode cannot be dragged back either.
    const int sizes[][2] = {
        {1280, 720},   // a typical phone in landscape
        {2400, 1080},  // a tall modern phone
        {2048, 1536},  // an iPad, much squarer
        {800, 480},    // a small handheld
        {720, 1280},   // held in portrait
    };

    for (const auto& size : sizes) {
        const int w = size[0];
        const int h = size[1];
        const float smaller = static_cast<float>(w < h ? w : h);

        for (const TouchControl& control : default_touch_layout(w, h)) {
            const float r = control.radius * smaller;
            const float cx = control.x * static_cast<float>(w);
            const float cy = control.y * static_cast<float>(h);

            CHECK(cx - r >= 0.0f);
            CHECK(cy - r >= 0.0f);
            CHECK(cx + r <= static_cast<float>(w));
            CHECK(cy + r <= static_cast<float>(h));
        }
    }
}

TEST(the_dpad_is_a_square_cross_not_a_stretched_diamond) {
    // x is a fraction of width and y a fraction of height, so laying a cross
    // out with equal fractional offsets makes it far wider than it is tall on
    // a widescreen phone. The four arms must be the same distance in PIXELS.
    const int w = 2400;
    const int h = 1080;
    const std::vector<TouchControl> layout = default_touch_layout(w, h);

    const TouchControl* up = find(layout, RETRO3DO_PAD_UP);
    const TouchControl* down = find(layout, RETRO3DO_PAD_DOWN);
    const TouchControl* left = find(layout, RETRO3DO_PAD_LEFT);
    const TouchControl* right = find(layout, RETRO3DO_PAD_RIGHT);
    CHECK(up != nullptr && down != nullptr && left != nullptr && right != nullptr);

    const float vertical = pixel_distance(*up, *down, w, h);
    const float horizontal = pixel_distance(*left, *right, w, h);

    // Within a couple of percent; the conversion is floating point.
    CHECK(std::fabs(vertical - horizontal) < vertical * 0.02f);
}

TEST(the_dpad_stays_square_on_a_squarer_screen_too) {
    const int w = 2048;
    const int h = 1536;
    const std::vector<TouchControl> layout = default_touch_layout(w, h);

    const float vertical = pixel_distance(*find(layout, RETRO3DO_PAD_UP),
                                          *find(layout, RETRO3DO_PAD_DOWN), w, h);
    const float horizontal = pixel_distance(*find(layout, RETRO3DO_PAD_LEFT),
                                            *find(layout, RETRO3DO_PAD_RIGHT), w, h);
    CHECK(std::fabs(vertical - horizontal) < vertical * 0.02f);
}

TEST(movement_is_on_the_left_and_actions_on_the_right) {
    const std::vector<TouchControl> layout = default_touch_layout(1280, 720);
    CHECK(find(layout, RETRO3DO_PAD_LEFT)->x < 0.4f);
    CHECK(find(layout, RETRO3DO_PAD_UP)->x < 0.4f);
    CHECK(find(layout, RETRO3DO_PAD_A)->x > 0.6f);
    CHECK(find(layout, RETRO3DO_PAD_B)->x > 0.6f);
    CHECK(find(layout, RETRO3DO_PAD_C)->x > 0.6f);
}

TEST(the_shoulder_buttons_are_up_out_of_the_way) {
    // They sit where a thumb does not rest, so they are not pressed by accident
    // while holding the device.
    const std::vector<TouchControl> layout = default_touch_layout(1280, 720);
    CHECK(find(layout, RETRO3DO_PAD_LEFT_SHIFT)->y < 0.3f);
    CHECK(find(layout, RETRO3DO_PAD_RIGHT_SHIFT)->y < 0.3f);
}

TEST(every_button_the_machine_has_is_present_exactly_once) {
    const std::vector<TouchControl> layout = default_touch_layout(1280, 720);
    const retro3do_pad_button expected[] = {
        RETRO3DO_PAD_UP, RETRO3DO_PAD_DOWN, RETRO3DO_PAD_LEFT, RETRO3DO_PAD_RIGHT,
        RETRO3DO_PAD_A, RETRO3DO_PAD_B, RETRO3DO_PAD_C,
        RETRO3DO_PAD_PLAY, RETRO3DO_PAD_STOP,
        RETRO3DO_PAD_LEFT_SHIFT, RETRO3DO_PAD_RIGHT_SHIFT,
    };
    for (retro3do_pad_button button : expected) {
        int count = 0;
        for (const TouchControl& control : layout) {
            if (control.button == button) ++count;
        }
        CHECK_EQ(count, 1);
    }
    CHECK_EQ(layout.size(), sizeof(expected) / sizeof(expected[0]));
}

TEST(controls_do_not_overlap_each_other) {
    // Overlapping controls make one of them unpressable, since a touch resolves
    // to exactly one.
    const int w = 1280;
    const int h = 720;
    const float smaller = static_cast<float>(h);
    const std::vector<TouchControl> layout = default_touch_layout(w, h);

    for (size_t i = 0; i < layout.size(); ++i) {
        for (size_t j = i + 1; j < layout.size(); ++j) {
            const float distance = pixel_distance(layout[i], layout[j], w, h);
            const float radii =
                (layout[i].radius + layout[j].radius) * smaller;
            CHECK(distance >= radii);
        }
    }
}
