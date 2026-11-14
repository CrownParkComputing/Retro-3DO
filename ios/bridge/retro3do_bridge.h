// The whole of the emulator, as plain C, for the Swift front end.
//
// Why this exists: the iOS app is SwiftUI. It owns the window, the picture and
// the sound, and it never sees a C++ type, an SDL window or an ImGui draw list.
// Everything it needs is below, and nothing below needs a window to work — the
// Console was already written to emulate into a buffer and return, so the whole
// of iOS presentation lives on the Swift side.
//
// Threading: r3_start() spawns one emulation thread and that thread is the only
// one that touches the Console. Everything here is safe to call from Swift's
// main actor; calls are queued onto the emulation thread or served from a
// mutex-protected snapshot. The picture is double-buffered: the emulation
// thread writes the back buffer and swaps, the display link copies the front.
#ifndef RETRO3DO_BRIDGE_H
#define RETRO3DO_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------- lifecycle --

// Create the machine and start its thread. `support_dir` is where the NVRAM
// (the 3DO's save memory) and the library index are kept — on iOS, Application
// Support, which is backed up and is not visible in the Files app.
//
// Returns false only if the machine itself could not be created. Having no
// BIOS is not a failure: the built-in demo runs without one.
bool r3_start(const char *support_dir);

// Stop the thread, flush the NVRAM and destroy the machine.
void r3_shutdown(void);

// ------------------------------------------------------------------ content --

// What r3_import() decided a file was.
typedef enum {
    R3_IMPORT_FAILED = 0,   // unreadable, or nothing this machine can use
    R3_IMPORT_BIOS   = 1,   // a 3DO system ROM; it is now loaded and remembered
    R3_IMPORT_DISC   = 2,   // a disc image; it is now in the library
} R3ImportKind;

// Take one file the user picked and work out what it is, so that the front end
// never has to ask. `out_name` receives the display name (the disc's own title
// where the image carries one, otherwise the file name).
//
// This is the only way content enters the app. There is no file browser and no
// typed path: iOS hands us a URL from the document picker and we decide.
R3ImportKind r3_import(const char *path, char *out_name, int out_name_len);

// Whether a system ROM has been supplied. Commercial discs need one; the demo
// does not. The front end uses this only to decide whether to explain.
bool r3_has_bios(void);

// Start the built-in demonstration ROM. It is original Crown Park Computing
// code, ships inside the app and needs no BIOS and no disc, which is why it is
// what a first launch offers.
bool r3_play_demo(void);

// Start a disc from the library. Needs a BIOS; returns false without one and
// r3_last_error() says so in words meant for a person.
bool r3_play_disc(const char *path);

// Leave the session and return to the shelf. The NVRAM is flushed here, so a
// game's saves survive even if iOS kills the app afterwards.
void r3_stop(void);

bool r3_is_playing(void);
void r3_set_paused(bool paused);
bool r3_is_paused(void);
void r3_reset(void);

// ------------------------------------------------------------------ library --

// How many discs the library holds, and the n-th one's display name and path.
// The library is a plain list kept beside the NVRAM; importing adds to it and
// r3_library_forget() removes an entry without touching the file.
int  r3_library_count(void);
int  r3_library_name(int index, char *out, int out_len);
int  r3_library_path(int index, char *out, int out_len);
void r3_library_forget(int index);

// ------------------------------------------------------------------ picture --

// Bumped once per emulated frame. The display link compares it with the last
// value it drew and copies only when it has changed, so a 60 Hz screen showing
// a 50 Hz PAL machine does no redundant work.
uint64_t r3_frame_serial(void);

// Copy the newest frame as XRGB8888. Returns the number of pixels written, or
// 0 when `capacity` is too small — in which case *out_w and *out_h still tell
// the caller how big a buffer to allocate.
int r3_copy_frame(uint32_t *dst, int capacity, int *out_w, int *out_h);

// The machine's pixels are not square. 1000 means square; the 3DO's 320x240
// over a 4:3 screen is 1000, and PAL's 320x288 is wider per pixel.
int r3_pixel_aspect_x1000(void);

// -------------------------------------------------------------------- sound --

// Pull interleaved stereo 16-bit samples at 44100 Hz. Returns frames written;
// a short read is silence-padded by the caller, which is what an audio unit
// wants when emulation is briefly behind.
int r3_pull_audio(int16_t *dst, int frames);

// --------------------------------------------------------------------- input --

// Typed rather than an anonymous enum: an anonymous enum's underlying type is
// the compiler's choice, so Swift may import these as Int32 or UInt32 and the
// mask arithmetic only compiles against one of them.
static const uint32_t R3_PAD_UP          = 1u << 0;
static const uint32_t R3_PAD_DOWN        = 1u << 1;
static const uint32_t R3_PAD_LEFT        = 1u << 2;
static const uint32_t R3_PAD_RIGHT       = 1u << 3;
static const uint32_t R3_PAD_A           = 1u << 4;
static const uint32_t R3_PAD_B           = 1u << 5;
static const uint32_t R3_PAD_C           = 1u << 6;
static const uint32_t R3_PAD_PLAY        = 1u << 7;    // the button marked P
static const uint32_t R3_PAD_STOP        = 1u << 8;    // the button marked X
static const uint32_t R3_PAD_LEFT_SHIFT  = 1u << 9;
static const uint32_t R3_PAD_RIGHT_SHIFT = 1u << 10;

// The whole controller at once. Swift sends the complete state on every change
// rather than press/release pairs, so a dropped touch cannot leave a button
// stuck down.
void r3_pad(uint32_t mask);

// ----------------------------------------------------------------- settings --

void r3_set_region(bool pal);
bool r3_region_is_pal(void);

// ------------------------------------------------------------------- guide --

// The first-run walkthrough. The words live in the core so that iOS, Android
// and the desktop all say the same thing; only the name of the control that
// adds content differs, and `add_control` supplies it.
int r3_guide_count(void);
int r3_guide_title(int step, char *out, int out_len);
int r3_guide_body(int step, const char *add_control, char *out, int out_len);

// 0xRRGGBB for the step. The colour lives beside the words so a step looks the
// same in every version of the app rather than being restyled three times.
uint32_t r3_guide_accent(int step);

// --------------------------------------------------------------- diagnostics --

// The last failure, phrased for a person rather than a log. Never NULL.
const char *r3_last_error(void);

// Emulated frames per second, times 100. The front end shows this only behind
// the settings sheet; it is not on the shelf.
int r3_fps_x100(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // RETRO3DO_BRIDGE_H
