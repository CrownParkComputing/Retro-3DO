// The first-run walkthrough, written once for every version of the app.
//
// iOS draws it as a sheet, Android and the desktop draw it in their own front
// end, and all three read these words. That is the point: the thing a new user
// is told about this machine should not drift between platforms, and nobody
// should have to find three copies of a sentence to fix a typo in it.
//
// Two things vary and are handled here rather than by each front end going its
// own way. The name of the control that adds content appears in the text as a
// {ADD} token. And getting games into the app is genuinely a different job on
// each platform -- Android grants a folder, the desktop points at one, iOS
// copies a file in -- so steps carry the platforms they belong to and the
// others never see them.
#pragma once

#include <cstddef>

namespace retro3do {

enum GuidePlatform : unsigned {
    kGuideIos        = 1u << 0,
    kGuideAndroid    = 1u << 1,
    kGuideDesktop    = 1u << 2,
    kGuideEverywhere = kGuideIos | kGuideAndroid | kGuideDesktop,
};

// What a step offers to do for the user, rather than only describe. Each front
// end maps these onto its own machinery -- a document picker on iOS, the
// storage framework on Android, a file browser on the desktop -- which is the
// only way one walkthrough can show three different apps how they work.
enum GuideAction : unsigned {
    kActionNone = 0,
    kActionPlayDemo,      // start the built-in demonstration there and then
    kActionAddGame,       // import a disc image: Downloads, Files, anywhere
    kActionAddBios,       // import a system ROM, filed wherever this platform keeps it
    kActionChooseFolder,  // grant or point at the folder games live in
    kActionEditLayout,    // move the on-screen controls where the hand wants them
};

struct GuideStep {
    const char* title;
    const char* body;
    // Shown instead of `body` when the thing this step asks for is already in
    // place -- a system ROM already loaded, games already found. Nobody should
    // be walked through importing what they have already got. null means the
    // step asks for nothing, so there is nothing to have done.
    const char* done_body;
    unsigned    platforms;
    // 0xRRGGBB, carried here so a step is the same colour in every front end.
    // Taken from the app's own spectrum rather than invented per platform.
    unsigned    accent;
    // Up to two things the step can do. Two because Android needs both: grant
    // a folder, or bring in a single file from Downloads.
    GuideAction action;
    const char* action_label;
    GuideAction action2;
    const char* action2_label;
};

// How many steps this platform shows, and the n-th of them.
std::size_t guide_step_count(unsigned platform);
const GuideStep* guide_step(unsigned platform, std::size_t index);

// `body` with {ADD} replaced by `add_control`. The caller owns nothing; the
// result is written into `out` and truncated rather than allocated, so this is
// safe to call from a draw loop.
void guide_body(const char* body, const char* add_control, char* out, std::size_t out_len);

}  // namespace retro3do
