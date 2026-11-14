#include "guide.h"

#include <cstring>

namespace retro3do {
namespace {

// The app's own spectrum, used as a sequence so the walkthrough warms from
// pink through amber to cyan as it goes. Each step keeps its colour in every
// front end, which is why the values live here and not in a stylesheet.
constexpr unsigned kPink   = 0xFA4F78;
constexpr unsigned kAmber  = 0xFD9E3D;
constexpr unsigned kCyan   = 0x4FD1E8;
constexpr unsigned kGreen  = 0x56C98D;
constexpr unsigned kViolet = 0xA98BF5;

// Written for somebody who has never used an emulator and does not know what a
// BIOS is. Nothing here names a file format unless the user has to type one,
// and no sentence explains the machine's internals for their own sake.
const GuideStep kSteps[] = {
    {
        "Welcome to Retro-3DO",
        "This is a 3DO Interactive Multiplayer, the CD console Panasonic and "
        "Goldstar sold from 1993. The app is the machine; the discs are yours.\n\n"
        "It opens on a shelf. Everything you can play is a tile on it, and "
        "tapping a tile starts it. There is nothing else to learn, and there is "
        "no account and no sign-in anywhere in the app.\n\n"
        "Where a step can do something for you, there is a button under it "
        "that does it.",
        nullptr,
        kGuideEverywhere, kPink, kActionNone, nullptr, kActionNone, nullptr
    },
    {
        "Start with the demo",
        "The first tile is a demonstration we wrote ourselves. It needs nothing "
        "from you: no disc, no system ROM, no setup.\n\n"
        "Start it and you will see the machine test its own parts \xE2\x80\x94 the "
        "processor, the video hardware, the sound chip and the controller \xE2\x80\x94 and "
        "then let you move a marker around with the pad. It is the quickest way "
        "to see whether everything is working.\n\n"
        "Try it now \xE2\x80\x94 this guide will still be here.",
        nullptr,
        kGuideEverywhere, kAmber, kActionPlayDemo, "Play the demo now",
        kActionNone, nullptr
    },

    // ---- getting games in: one step each, and only one is ever shown -------

    {
        "Getting a game onto your iPhone",
        "A 3DO game is a disc image: one file ending .iso, .chd or .img, or a "
        ".bin with a .cue file beside it. Use images of discs you own.\n\n"
        "Get the file into Files first \xE2\x80\x94 Downloads is the usual place, and "
        "iCloud Drive or a plugged-in USB drive work just as well. iOS keeps "
        "every app's files to itself, so there is no folder you can point this "
        "app at; the file has to be brought in.\n\n"
        "Then choose {ADD}. The picker opens on Files, you pick the image, and "
        "it is copied into the app and becomes a tile. Because it is a copy it "
        "keeps working even if you move or delete the original.\n\n"
        "There is no folder to choose and nothing to set up: iOS gives every "
        "app one place of its own and that is where your games go.\n\n"
        "For a .bin and .cue pair, pick the .cue and keep the .bin beside it.",
        "Your games are already here, on the shelf behind this guide.\n\n"
        "Add more whenever you like with {ADD} \xE2\x80\x94 the picker opens on Files, and "
        "what you choose is copied in beside the rest.",
        kGuideIos, kCyan, kActionAddGame, "Import a game", kActionNone, nullptr
    },
    {
        "Where to put your games",
        "A 3DO game is a disc image: one file ending .iso, .chd or .img, or a "
        ".bin with a .cue file beside it. Use images of discs you own.\n\n"
        "Put them in one folder of your own \xE2\x80\x94 Downloads will do, or make a "
        "folder called 3DO and keep them together. Subfolders inside it are "
        "fine.\n\n"
        "Then point Retro-3DO at that folder. Android will ask you to confirm, "
        "and the app is given that folder and nothing else. You only do this "
        "once: the permission is remembered between launches, the app can never "
        "see anything outside the folder you chose, and nothing is copied \xE2\x80\x94 "
        "your files stay where they are.",
        "Your games folder is already set and the app can see what is in it, so "
        "there is nothing to do here.\n\n"
        "Drop new disc images into that folder and they appear. You can point "
        "the app somewhere else at any time.",
        kGuideAndroid, kCyan, kActionChooseFolder, "Choose my games folder",
        kActionAddGame, "Import one game"
    },
    {
        "Where to put your games",
        "A 3DO game is a disc image: one file ending .iso, .chd or .img, or a "
        ".bin with a .cue file beside it. Use images of discs you own.\n\n"
        "Keep them in one folder \xE2\x80\x94 Downloads will do, or a folder of their "
        "own \xE2\x80\x94 and point Retro-3DO at it. It reads them where they sit, "
        "subfolders included. Nothing is copied and nothing is moved.\n\n"
        "You can change the folder later, or add a single disc with {ADD} if "
        "you would rather not hand over a whole folder.",
        "Your games folder is already set and the app can see what is in it, so "
        "there is nothing to do here.\n\n"
        "Drop new disc images into that folder and they appear. You can point "
        "the app somewhere else at any time.",
        kGuideDesktop, kCyan, kActionChooseFolder, "Choose my games folder",
        kActionAddGame, "Import one game"
    },

    {
        "About the system ROM",
        "The demo plays without one, but a real 3DO game needs the console's "
        "own start-up software, which we are not allowed to include.\n\n"
        "If you have it \xE2\x80\x94 most people take it from a console they own \xE2\x80\x94 add it "
        "the same way you added your discs. The app recognises it on sight and "
        "remembers it. You only ever do this once, and until you do, your discs "
        "will tell you they need it.",
        "You already have a system ROM, so your discs will play. It is remembered, "
        "and you will not be asked for it again.",
        kGuideEverywhere, kViolet, kActionAddBios, "Import my system ROM",
        kActionNone, nullptr
    },
    {
        "Put the controls where you want them",
        "The on-screen pad starts where it suits most hands: the directional "
        "cross on the left, A, B and C on the right in that order, and the "
        "shoulder buttons out of the way at the top.\n\n"
        "Hands differ. Every control can be dragged wherever you want it and "
        "the arrangement is remembered, so it is worth a minute now rather "
        "than fighting it later.",
        nullptr,
        kGuideIos | kGuideAndroid, kViolet, kActionEditLayout,
        "Arrange my controls", kActionNone, nullptr
    },
    {
        "The controls",
        "The 3DO pad is drawn either side of the picture, never over it: the "
        "directional cross on one side, and A, B and C on the other.\n\n"
        "P and X are the buttons marked P and X on the real pad \xE2\x80\x94 most games "
        "use P to pause and start. L and R are the shoulder buttons.\n\n"
        "Touch the picture to hide the controls and see the game on its own. "
        "Touch it again to bring them back.",
        nullptr,
        kGuideIos | kGuideAndroid, kGreen, kActionNone, nullptr, kActionNone, nullptr
    },
    {
        "The controls",
        "A game pad plugged in or paired is picked up automatically and mapped "
        "to the 3DO pad: the stick or d-pad to the cross, and three face "
        "buttons to A, B and C.\n\n"
        "Without one, the keyboard stands in \xE2\x80\x94 the arrow keys steer, and Z, X "
        "and C are A, B and C.",
        nullptr,
        kGuideDesktop, kGreen, kActionNone, nullptr, kActionNone, nullptr
    },
    {
        "Your games save themselves",
        "The 3DO kept saved games in a small memory inside the console, and so "
        "does this. It is written whenever you stop playing or leave the app, "
        "so you will not lose a save by closing it.\n\n"
        "Saves belong to the machine, not to a disc, exactly as they did on the "
        "real console.",
        nullptr,
        kGuideEverywhere, kAmber, kActionNone, nullptr, kActionNone, nullptr
    },
    {
        "If something runs too fast",
        "A few discs were made for European televisions and run at a different "
        "speed. If a game feels too fast or too slow, change the television "
        "setting from 60 Hz to 50 Hz.\n\n"
        "That is the only setting most people ever need. You can read this "
        "guide again whenever you like \xE2\x80\x94 it is the first thing in Settings.",
        nullptr,
        kGuideEverywhere, kPink, kActionNone, nullptr, kActionNone, nullptr
    },
};

constexpr std::size_t kTotal = sizeof(kSteps) / sizeof(kSteps[0]);

}  // namespace

std::size_t guide_step_count(unsigned platform) {
    std::size_t n = 0;
    for (std::size_t i = 0; i < kTotal; ++i) {
        if ((kSteps[i].platforms & platform) != 0u) ++n;
    }
    return n;
}

const GuideStep* guide_step(unsigned platform, std::size_t index) {
    std::size_t n = 0;
    for (std::size_t i = 0; i < kTotal; ++i) {
        if ((kSteps[i].platforms & platform) == 0u) continue;
        if (n == index) return &kSteps[i];
        ++n;
    }
    return nullptr;
}

void guide_body(const char* body, const char* add_control, char* out, std::size_t out_len) {
    if (out == nullptr || out_len == 0) return;
    out[0] = '\0';
    if (body == nullptr) return;
    const char* token = "{ADD}";
    const std::size_t token_len = std::strlen(token);
    const char* replacement = add_control != nullptr ? add_control : "Add";
    const std::size_t replacement_len = std::strlen(replacement);

    std::size_t written = 0;
    for (const char* p = body; *p != '\0';) {
        const char* hit = std::strstr(p, token);
        const char* copy_end = hit != nullptr ? hit : p + std::strlen(p);
        for (const char* q = p; q < copy_end && written + 1 < out_len; ++q) {
            out[written++] = *q;
        }
        if (hit == nullptr) break;
        for (std::size_t i = 0; i < replacement_len && written + 1 < out_len; ++i) {
            out[written++] = replacement[i];
        }
        p = hit + token_len;
    }
    out[written] = '\0';
}

}  // namespace retro3do
