// The first-run walkthrough, written once for every version of the app.
//
// iOS draws it as a sheet, Android and the desktop draw it in their own front
// end, and all three read these words. That is the point: the thing a new user
// is told about this machine should not drift between platforms, and nobody
// should have to find three copies of a sentence to fix a typo in it.
//
// What varies between platforms is how content is added, so the bodies carry
// one token, {ADD}, which each front end replaces with the name of its own
// control: "the + tile" on iOS, "Add disc" on the desktop.
#pragma once

#include <cstddef>

namespace retro3do {

struct GuideStep {
    const char* title;
    const char* body;
};

// Steps in the order a first-time user meets them.
const GuideStep* guide_steps();
std::size_t guide_step_count();

// `body` with {ADD} replaced by `add_control`. The caller owns nothing; the
// result is written into `out` and truncated rather than allocated, so this is
// safe to call from a draw loop.
void guide_body(const char* body, const char* add_control, char* out, std::size_t out_len);

}  // namespace retro3do
