# Retro-3DO

A new 3DO Interactive Multiplayer emulator codebase for phones, tablets and
desktops. Its historical reference exposure is disclosed in
[core/docs/PROVENANCE.md](core/docs/PROVENANCE.md).

One C++ codebase builds for Android, iOS, macOS, Linux and Windows. There is no
per-platform front end: SDL provides the window, input, audio and GPU on every
target, and Dear ImGui draws the launcher and the in-game overlay.

> **Status: games run.** A real 3DO BIOS passes its power-on self test, CD media
> is mounted through XBUS/MADAM DMA, and the ARM, CEL, matrix and DSP paths run
> commercial titles. Compatibility is still in progress, but Road Rash and The
> Need for Speed both boot into correctly rendered gameplay. See
> [Roadmap](#roadmap) and the [Opera parity audit](core/docs/OPERA_AUDIT.md).

## Why this exists as a separate core

Two reasons, and the second is the one with a deadline attached.

**It is written to be fast on a phone.** The widely-used 3DO cores descend from
a 1990s desktop emulator. Their CPU is a plain interpreter that re-decodes every
instruction on every execution, their graphics chip is scalar C, and they
present frames from inside the emulation thread — so a slow present stalls
emulation, and the machine appears to run slow when it is really being
throttled. This core decodes each instruction once and caches it, and never
presents from the emulation path at all.

**Its current architecture is project-owned and auditable.** The former
direct-port routines have been replaced with implementations structured around
public ARM/3DO documentation, patents and focused tests. The project is still
historically reference-exposed; replacing code does not make its development
history clean-room. [core/docs/PROVENANCE.md](core/docs/PROVENANCE.md) records both facts.

## Building

### Linux and Windows

```sh
cmake -S . -B build -DRETRO3DO_USE_SYSTEM_SDL=ON
cmake --build build -j
./build/retro3do
```

Choose **Demo** in the launcher, or run `./build/retro3do --demo`, to boot the
included original hardware demonstration without a commercial BIOS or game.
The 1.7 KB bare-metal ARM ROM drives MADAM/VDLP video, DSPP audio and PBUS input
through the emulator's normal hardware paths. Its complete source and
reproducible build are in [`demo/`](demo/).

Drop `-DRETRO3DO_USE_SYSTEM_SDL=ON` to build the vendored SDL from source
instead, which is what the mobile targets do.

### Tests

```sh
./build/retro3do_tests
```

They run on the host, with no window and no device. A bug found on hardware gets
reproduced here before it gets fixed.

### Android

```sh
cd android
./gradlew :app:bundleDebug -PBUILD_WITH_CMAKE=1
```

Gradle drives the same root `CMakeLists.txt` the desktop build uses; there is no
separate Android copy of the emulator.

On Android the app requests the fastest full-resolution panel mode up to
120 Hz and keeps SDL presentation vsynced to it. The 3DO itself remains at its
hardware-correct 50/60 Hz field rate; presentation refresh never changes game
or audio speed. A 60 Hz-only panel therefore remains at 60 Hz.

The Settings panel and in-game overlay offer optional 125% and 150% ARM boosts.
Only the emulated ARM instruction budget changes: video fields remain at 50/60
Hz, DSP output remains 44.1 kHz, and CD timing remains native. This can reduce
CPU-bound slowdown without fast-forwarding sound or FMV; it cannot raise a
frame rate deliberately capped by the game.

The Android Settings page can sign in to a registered RetroMedia account and
download one selected artwork piece per matched 3DO title (2D cover,
screenshot, thumbnail or title screen). Sessions are encrypted with the
Android Keystore; passwords are never written to the emulator settings. Media
is resized and cached in private app storage, then shown directly on library
cards. Downloads happen only when the user presses **Download Missing
Artwork**, and use that account's normal daily allowance/credit balance.

### macOS

```sh
macos/build-macos.sh          # universal Retro-3DO.app
```

macOS-only, and deliberately so — a Linux cross-build can make something that
sideloads but never something submittable, and `actool` (the app icon) and
`codesign` exist nowhere else.

It has not been run: everything else in this repository is built and tested from
Linux. [docs/APPLE_BUILD.md](docs/APPLE_BUILD.md) is the handover — what is set
up, what to expect on the first real Xcode run, and how to sign and notarise.

### iOS

The iPhone and iPad application is **not here**. It is
[Clio](https://github.com/CrownParkComputing/Clio), its own repository and its
own application, written in Swift against the same core: this repository builds
the Android and desktop applications, Clio builds the iOS one, and both carry
`core/` as the same submodule pinned to the same commit.

That is not tidiness. Apple rejected the family under guideline 4.3 because the
apps were one workbench shipped several times with a different accent colour, so
the iOS app has to be a genuinely different application rather than this one
with a new icon — which means its own interface, its own repository, and no
shared UI code at all.

An iOS configure of *this* project fails on purpose, with a message naming Clio,
rather than stopping later on a missing `Info.plist` that reads like a corrupted
checkout.

## Layout

```
core/           submodule: the 3DO itself, reached only through retro3do.h
src/app/        the front end's own non-UI parts: settings, paths, pad layout
src/platform/   SDL: window, events, presentation
src/ui/         Dear ImGui: launcher and overlay
tests/          host-side tests for what this repository owns
android/        Gradle shell around the shared CMake tree
macos/          the same, for the desktop bundle: plist, entitlements, script
assets/apple/   asset catalogue, compiled into the icon by actool
```

`core/` is [retro3do-core](https://github.com/CrownParkComputing/retro3do-core),
which used to be `src/core/` here. It moved out because a second application --
[Clio](https://github.com/CrownParkComputing/Clio), the iOS one -- needs the
same emulator, and a core that only builds inside one app gets copied into the
other and then the two drift apart.

### The boundary

This application reaches the core through **`retro3do.h` and nothing else**. It
is a plain C interface over an opaque handle, and the core's CMake target
exports only its `include/` directory, so a machine header cannot be included
from this side even by accident. That is deliberate: the other application is
written in Swift, and an interface only one of the two can consume is not an
interface.

What that means in practice:

- **The core owns the emulator thread and the pacing policy.** The application
  asks it to start, stop and pause; it does not own a thread of its own. The
  previous 3DO core stacked three independent frame-droppers on top of each
  other and reported itself healthy while dropping most of its frames -- one
  policy in one place is the fix, and two front ends each carrying a copy of it
  is how that fix would be lost.
- **The machine's tests live with the machine.** ARM60, the bus, MADAM, CLIO,
  VDLP, SPORT, the DSP, discs and CHD are checked in the core repository, along
  with a suite that drives `retro3do.h` exactly as an application does. What is
  tested here is what this repository owns.
- **Instruments go with the core too.** `gamecheck` and the Opera oracle read
  the machine's internals, which is what makes them useful and what no front end
  may do.
- If something in `core/` ever needs a platform header, it belongs in
  `src/platform/` instead. If the application needs something the core will not
  tell it, the answer is to add it to `retro3do.h` -- never to widen the include
  path.

### Checking the front end without a screen

The unit tests cannot tell you that frames are reaching the window, because a
front end is the one part of an emulator they cannot reach. This can:

```sh
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
    ./retro3do --selftest 10 /path/to/bios.bin /path/to/disc.chd
```

It runs the whole application -- window, event pump, emulator thread,
presentation -- for ten seconds with nobody in front of it and reports frames
emulated, frames displayed, and how much of the last picture is content rather
than one flat colour. It exits non-zero if the machine emulated nothing or never
drew anything, so a build machine with no display can still tell a working
application from one that comes up blank.

## Roadmap

- [x] ARM60 CPU with a decoded-instruction cache
- [x] Memory map and big-endian bus
- [x] SDL application shell, ImGui launcher
- [x] Android and iOS build paths off one CMake tree
- [x] macOS bundle target, icons and signing set up (unrun: see docs/APPLE_BUILD.md)
- [x] CLIO — interrupts, timers, video counters
- [x] VDLP — display list to framebuffer
- [x] MADAM — the CEL engine: affine mapping, indexed and direct cels
- [x] DSP — instruction engine, DMA inputs and DAC output
- [x] Disc images: ISO, BIN, CUE, with sector layout detected not assumed
- [x] SPORT — page copy and fill; the ROM does not pass its self test without it
- [x] XBUS + MADAM DMA: animated startup and disc loading
- [x] Emulation on its own thread, with one pacing policy and a triple-buffered frame mailbox
- [x] Audio output path: lock-free ring, SDL sink
- [x] Controller: keyboard and gamepad through SDL
- [x] File browser, so the app is usable without a keyboard
- [x] Android scoped storage (SAF): the user grants folders, the app requests no storage permission
- [x] On-screen controls, movable, saved per device
- [x] Settings that survive a restart
- [x] Original built-in no-BIOS hardware demonstration ROM
- [x] RetroMedia account artwork downloads and illustrated library cards (Android)
- [x] Scanline-timed VDLP updates
- [ ] Full MADAM CEL pause/status timing
- [ ] Remaining CD queries, CD audio and additional PBUS peripherals
- [ ] Save states

## Licence

Project-owned code is MIT licensed; see [LICENSE](LICENSE). Bundled components
retain their own terms, collected in [core/docs/THIRD_PARTY.md](core/docs/THIRD_PARTY.md).
The distribution and historical reference-source notices in [NOTICE](NOTICE)
and [core/docs/PROVENANCE.md](core/docs/PROVENANCE.md) are part of the release record.
