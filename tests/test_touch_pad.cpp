#include "ui/touch_pad.h"
#include "test_harness.h"

#include <SDL3/SDL.h>

using namespace retro3do;

TEST(touch_layout_editor_works_with_a_physical_controller_and_touch_disabled) {
    TouchPad touch;
    PadState pads;
    touch.set_visible(false);
    touch.set_physical_gamepad_present(true);
    CHECK(!touch.visible());
    touch.set_editing(true);
    CHECK(touch.visible());

    const TouchControl original = touch.controls().front();
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.x = original.x * 1280.0f;
    event.button.y = original.y * 720.0f;
    CHECK(touch.handle_event(event, 1280, 720, pads));
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.x = 640.0f;
    event.motion.y = 360.0f;
    CHECK(touch.handle_event(event, 1280, 720, pads));
    CHECK(touch.controls().front().x == 0.5f);
    CHECK(touch.controls().front().y == 0.5f);
    CHECK(!pads.pressed(0, original.button));

    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    CHECK(touch.handle_event(event, 1280, 720, pads));
    touch.set_editing(false);
    CHECK(!touch.enabled());
    CHECK(!touch.visible());
}

TEST(connecting_a_controller_keeps_the_layout_editor_open) {
    TouchPad touch;
    touch.set_visible(true);
    touch.set_editing(true);
    touch.set_physical_gamepad_present(true);
    CHECK(touch.editing());
    CHECK(touch.visible());
    touch.set_editing(false);
    CHECK(!touch.visible());
    touch.set_physical_gamepad_present(false);
    CHECK(touch.visible());
}
