# Long-form working notes for the Retro-3DO 4.3 reply

The paste-ready text is `resolution-reply-4.3-answers.txt` (3,952 characters
including the feedback paragraph, against Apple's 4,000-character field). This
file is the reasoning and the sources behind it. Nothing here goes to Apple.

## The situation

- App id `6808591874`, bundle id `com.crownparkcomputing.retro3DO`.
- Version 1.0, `02773cdd-50a5-4f21-a861-97c082f27417`, state **REJECTED**.
- Submission `73e3c902`, 2026-09-29. Rejected 2026-10-06 under **4.3 (Spam)
  and 4.2.6 (App Generation Services)**.
- **Appealed. Denied.** 2026-11-03, Appeal Ticket APL601620: *"the previous
  rejection feedback was valid"*.
- **No reply has been posted.** The thread `12f8a283-9b03-363e-8593-8662302a0fd2`
  (`canDeveloperAddNote: true`) holds only Apple's two messages.
- Apple's instruction is explicit: *"do not resubmit the app. Instead, reply to
  this message and answer all of the following questions in full"*.

## Why the reply and not a resubmit

DOSDeck got the identical rejection twice and Jon replied with the nine
answers; on 2026-10-23 Apple answered *"Thank you for providing this
information. We will continue the review"* and **4.3 was cleared**. The appeal
route is what failed here. The Stunt Car reply is at
`~/projects/StuntCarRacer-Godot/game/ios/resolution-reply-4.3-answers.txt` and
is the model for this one.

There is **no API route** to the Resolution Center. `asc web review show` reads
the thread; nothing posts a reply. It is typed by hand in App Store Connect,
which needs Jon's live 2FA.

## Sources for each answer

**1 — what it does.** The 3DO is a 1993 CD-ROM console whose best-known games
are the light-gun arcade titles. `README.md`: *"A new 3DO Interactive Multiplayer
emulator codebase for phones, tablets and desktops"*; *"There is no per-platform
front end: SDL provides the window, input, audio and GPU on every target, and
Dear ImGui draws the launcher and the in-game overlay"*; and the reason it
exists separately — *"The widely-used 3DO cores descend from a 1990s desktop
emulator. Their CPU is a plain interpreter that re-decodes every instruction on
every execution, their graphics chip is scalar C, and they present frames from
inside the emulation thread — so a slow present stalls emulation, and the
machine appears to run slow when it is really being throttled. This core decodes
each instruction once and caches it, and never presents from the emulation path
at all."*

**2 — intended user.** README status: *"A real 3DO BIOS passes its power-on
self test, CD media is mounted through XBUS/MADAM DMA, and the ARM, CEL, matrix
and DSP paths run commercial titles. Compatibility is still in progress, but
Road Rash and The Need for Speed both boot into correctly rendered gameplay."*

**3 — the gap.** The same README passage, plus `core/docs/PROVENANCE.md` for
the authority the implementation is built against. The pixel-identical claim
is `docs/HANDOVER.md`: *"Two titles are pixel-identical to the reference at
frame 20,000: Killing Time and Alone in the Dark 2"*; and *"250 tests pass"*.
Every chip CRC before a game starts is the import wizard's behaviour.

**4 — beta testing.** TestFlight, from the App Store Connect API: an External
group with a public link (limit 20) and an Internal group. Three testers, state
INSTALLED, all on build `1.0 (202609290702)` — the build that was submitted —
on an **iPad14_3** and an **iPhone17_2**, both **iOS 27.2**. `asc testflight
feedback list --app 6808591874` returns **zero feedback submissions**, so the
tester comment has to come from Jon's own record of what testers wrote.
Device testing on a Retroid handheld is in `docs/HANDOVER.md`; the host
regression suite is the 250 checks it describes.

**5 — standalone.** The account's eleven apps, from `asc apps list`: C64-Retro
(`com.vicemultiplatform.app`, a third party's app, disclosed), DOSDeck, Dad's
Wire Drawer, Retro-3DO, Retro-Amiga, Retro-Saturn, Retro-Spectrum, ST Desk,
SplatLand, Stunt Car Racing 2026, Virtua Cop Collection.

**6 — no IAP elsewhere.** The app has no network code at all. The `INTERNET`
permission existed only for RetroMedia and was removed on 2026-11-13 (commit
`b019ec0` on `ios-render-fixes`, `215dfe8` on `feature/remove-retromedia`).

**7 — shared code.** Verified by digest, not by eye: no file in
`src/ui/` of the iOS tree is byte-identical to any file in Retro-Saturn's
`frontend/` — the launcher, the on-screen controller, the file browser and the
game library are all Retro-3DO's own. The third-party libraries are listed in
the app's own `NOTICE` and `docs/THIRD_PARTY.md`: SDL3 (zlib), Dear ImGui
(MIT), libchdr (BSD-3-Clause), AdrenoTools (BSD-2-Clause), miniz (MIT), the
LZMA SDK (public domain), zstd (BSD).

**8 — third-party codebase.** `core/docs/PROVENANCE.md`: *"Retro-3DO is not a
clean-room project. During development, FreeDO-descended Opera code … was read
and used as a compatibility reference. … Replacing those routines later does
not undo that prior exposure, so this repository must not claim that it was
independently developed without knowledge of FreeDO or Opera."* The current
implementations are rebuilt against ARM DDI 0100I, the 3DO SDK, the Graphics
Programming Guide and the granted patents WO 94/10641, WO 94/10644, WO 94/16383,
WO 94/16382, WO 94/10636, US 5,502,462 and US 5,742,778.

**9 — no client.** The app is Crown Park Computing's own product, MIT licensed,
with the third-party libraries above used under their licences.

## What has to happen, in order

1. **Jon fills in answer 4** and posts the whole text as a reply in the
   Resolution Center thread for submission `73e3c902`.
2. **Apple answers.** Do not resubmit before it does — on this account the
   reply is what cleared 4.3 for DOSDeck.
3. **Build a new version.** The iOS build tree is the **`ios-render-fixes`
   branch** of `~/StudioProjects/Retro-3DO`; `main` is Android and desktop
   only (commit dba47c2, *"iOS is Clio, and this repository is Android and
   desktop"*), and the future iOS app is **Clio**, whose app-store record does
   not exist yet. The build has to come from the MacinCloud Mac.
4. **Retarget the rejected record.** A new version cannot be created while
   one is REJECTED, so rename the existing record and attach the new build:
   `asc versions update --version-id 02773cdd-… --version 1.1` then
   `asc versions attach-build --version-id 02773cdd-… --build-id <new>`.
   Attaching a fresh build flips REJECTED → PREPARE_FOR_SUBMISSION on its own.
5. **Recapture the screenshots.** The four in `store/appstore/screenshots/`
   show the navigation row with an **ARTWORK** button that the RetroMedia
   removal deleted; the row is now one button shorter.
6. **Update the review notes.** They still say *"Tap Demo in the left-hand
   navigation rail"*; the current layout is a top navigation row. The 4.7.4
   index of embedded software also needs the RetroMedia line gone — there was
   never a RetroMedia entry in it, so that is already true.
7. **Then submit**, with the two-call dance:
   `asc review submit --app 6808591874 --version 1.1 --build <id> --confirm`
   (it will report *"does not contain target version"* — that is eventual
   consistency, not a failure) and then
   `asc review submissions-submit --id <submission-id> --confirm`.

## Still unknown

- The tester feedback for answer 4. Nothing is in TestFlight, and the
  September-era rejection thread carries no developer reply to mine.
- Whether the shipped iOS build was taken from `ios-render-fixes` or from a
  branch that has since been deleted. The build number `202609290702` is a
  timestamp, and the branch tip is 2026-09-14.
