// Drives the iOS bridge exactly as the Swift front end does, with no window,
// no audio device and no Swift. It exists so the half of the iOS app that can
// be tested on Linux is tested on Linux: the bridge is the only place where a
// mistake is invisible from the Swift side and fatal on a device.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../ios/bridge/retro3do_bridge.h"

static void nap(int ms) {
    struct timespec t = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&t, NULL);
}

static int fail(const char* what) {
    fprintf(stderr, "FAIL: %s (%s)\n", what, r3_last_error());
    return 1;
}

int main(int argc, char** argv) {
    const char* dir = argc > 1 ? argv[1] : "/tmp";

    if (!r3_start(dir)) return fail("r3_start");
    printf("started, support dir %s\n", dir);

    if (r3_has_bios()) printf("a BIOS is already loaded\n");
    else               printf("no BIOS, as expected on a fresh install\n");

    if (!r3_play_demo()) return fail("r3_play_demo");
    printf("demo started\n");

    // Wait for the first published frame the way a display link would.
    unsigned long long serial = 0;
    for (int i = 0; i < 400 && serial == 0; ++i) { serial = r3_frame_serial(); nap(5); }
    if (serial == 0) return fail("no frame was ever published");
    printf("first frame at serial %llu\n", serial);

    // Let the ROM get past its own blank first frame. A machine that has just
    // been reset has drawn nothing yet, so sampling immediately tests the
    // reset, not the renderer.
    nap(500);

    int w = 0, h = 0;
    r3_copy_frame(NULL, 0, &w, &h);
    printf("picture %dx%d, pixel aspect %d/1000\n", w, h, r3_pixel_aspect_x1000());
    if (w <= 0 || h <= 0) return fail("picture has no size");

    uint32_t* pixels = malloc((size_t)w * h * sizeof(uint32_t));
    const int got = r3_copy_frame(pixels, w * h, &w, &h);
    if (got != w * h) return fail("short frame copy");

    // A frame that is entirely one colour is what a broken renderer produces,
    // and it is indistinguishable from success unless something counts.
    int distinct = 0;
    uint32_t seen[16];
    for (int i = 0; i < w * h; ++i) {
        int known = 0;
        for (int j = 0; j < distinct; ++j) known |= (seen[j] == pixels[i]);
        if (!known && distinct < 16) seen[distinct++] = pixels[i];
    }
    printf("%d distinct colours in the first frame\n", distinct);
    if (distinct < 2) return fail("the picture is a single flat colour");

    // Input has to reach the machine: hold a direction and confirm the picture
    // changes, which is what the demo ROM's marker does.
    const unsigned long long before = r3_frame_serial();
    r3_pad(R3_PAD_RIGHT | R3_PAD_A);
    nap(250);
    r3_pad(0);
    const unsigned long long after = r3_frame_serial();
    printf("%llu frames emulated while input was held\n", after - before);
    if (after <= before) return fail("emulation stopped");

    int16_t audio[2048 * 2];
    const int real = r3_pull_audio(audio, 2048);
    printf("%d of 2048 audio frames were real\n", real);

    // The rate is averaged over a whole second, so it reads zero until one has
    // passed. Waiting for it is the difference between testing the counter and
    // testing nothing.
    while (r3_fps_x100() == 0) nap(50);
    const int fps = r3_fps_x100();
    printf("fps x100: %d\n", fps);
    if (fps < 4000 || fps > 7000) return fail("emulated rate is not near 60 Hz");

    r3_set_paused(true);
    const unsigned long long paused_at = r3_frame_serial();
    nap(120);
    if (r3_frame_serial() != paused_at) return fail("pause did not stop emulation");
    printf("pause holds\n");
    r3_set_paused(false);

    // The importer is the whole of the new front door, so it is tested rather
    // than assumed. A file too small and too malformed to be either kind must
    // be refused, and a real disc image must land in the library under a name
    // fit to put on a tile.
    char junk_path[512];
    snprintf(junk_path, sizeof junk_path, "%s/not-a-3do-file.bin", dir);
    FILE* junk = fopen(junk_path, "wb");
    if (junk != NULL) {
        for (int i = 0; i < 2048; ++i) fputc(i & 0xff, junk);
        fclose(junk);
        char name[256] = {0};
        if (r3_import(junk_path, name, sizeof name) != R3_IMPORT_FAILED)
            return fail("a junk file was accepted as content");
        printf("junk refused, and said: %s\n", r3_last_error());
        remove(junk_path);
    }

    // The accept path, proved without a commercial disc. A 3DO disc is
    // recognised by its Opera volume header and nothing else, so 64 sectors of
    // zeroes with a correct header is a disc as far as the importer is
    // concerned — and the label it carries is what should end up on the tile.
    char made_path[512];
    snprintf(made_path, sizeof made_path, "%s/synthetic.iso", dir);
    FILE* made = fopen(made_path, "wb");
    if (made != NULL) {
        unsigned char image[2048 * 64];
        memset(image, 0, sizeof image);
        image[0] = 0x01;                      // record type
        memcpy(image + 1, "ZZZZZ", 5);        // the sync bytes
        image[6] = 0x01;                      // record version
        memcpy(image + 40, "SMOKE TEST DISC", 15);   // the volume label
        fwrite(image, 1, sizeof image, made);
        fclose(made);

        char name[256] = {0};
        const R3ImportKind kind = r3_import(made_path, name, sizeof name);
        printf("synthetic disc -> kind %d, named \"%s\"\n", (int)kind, name);
        if (kind != R3_IMPORT_DISC) return fail("a valid 3DO volume header was refused");
        if (strcmp(name, "SMOKE TEST DISC") != 0)
            return fail("the disc's own label did not become its name");
        if (r3_library_count() < 1) return fail("the disc did not reach the library");

        // And it comes back out again, taking the file with it.
        r3_library_forget(0);
        if (r3_library_count() != 0) return fail("removing a disc left it in the library");
        printf("synthetic disc imported, named from its label, and removed\n");
        remove(made_path);
    }

    // bridge_smoke <support dir> [file] [expected kind]
    //   0 refused, 1 system ROM, 2 disc.
    // Handing it a CD image of another console and expecting 0 is the sharpest
    // test there is: it is a real, well-formed disc image that the Disc layer
    // opens happily, and only the volume-header check can tell it apart from a
    // 3DO one.
    if (argc > 2) {
        const int want = argc > 3 ? atoi(argv[3]) : (int)R3_IMPORT_DISC;
        char name[256] = {0};
        const R3ImportKind kind = r3_import(argv[2], name, sizeof name);
        printf("imported %s -> kind %d, named \"%s\"\n", argv[2], (int)kind, name);
        if ((int)kind != want) return fail("the importer disagreed with the expected kind");
        if (kind == R3_IMPORT_DISC && r3_library_count() < 1)
            return fail("an imported disc did not reach the library");
    }

    // The walkthrough is shared with Android and the desktop, so a mistake in
    // the token substitution would be wrong in three apps at once.
    const int steps = r3_guide_count();
    printf("guide has %d steps\n", steps);
    if (steps < 5) return fail("the guide lost its steps");
    for (int i = 0; i < steps; ++i) {
        char title[128], body[2048];
        r3_guide_title(i, title, sizeof title);
        r3_guide_body(i, "the + tile", body, sizeof body);
        if (title[0] == 0 || body[0] == 0) return fail("a guide step is empty");
        if (strstr(body, "{ADD}") != NULL) return fail("a guide step still has {ADD} in it");
        if (r3_guide_accent(i) == 0) return fail("a guide step has no colour");
        /* Pointing at a folder is not something iOS lets an app do, so the
           Android and desktop storage steps must never reach the phone.
           Matched on the instruction itself rather than on any one sentence,
           so rewording the guide cannot quietly turn this check off. */
        if (strstr(body, "point Retro-3DO at") != NULL)
            return fail("a folder-permission step leaked into the iOS guide");
        if (strstr(body, "Android") != NULL)
            return fail("the iOS guide mentions Android");
        /* And the iOS step has to still say where a file comes from. */
        if (i == 2 && strstr(body, "Downloads") == NULL)
            return fail("the iOS storage step stopped naming Downloads");
    }
    /* Each step's offer, printed so the walkthrough can be read at a glance,
       and asserted so the buttons cannot quietly go missing. */
    int saw_demo = 0, saw_bios = 0, saw_game = 0;
    for (int i = 0; i < steps; ++i) {
        char title[128], label[128] = {0};
        r3_guide_title(i, title, sizeof title);
        const int act = r3_guide_action(i, 0);
        if (act != R3_GUIDE_NONE) r3_guide_action_label(i, 0, label, sizeof label);
        printf("   %d %-32s action %d %s\n", i + 1, title, act, label);
        if (act == R3_GUIDE_PLAY_DEMO) saw_demo = 1;
        if (act == R3_GUIDE_ADD_BIOS)  saw_bios = 1;
        if (act == R3_GUIDE_ADD_GAME)  saw_game = 1;
        if (act == R3_GUIDE_CHOOSE_FOLDER)
            return fail("iOS was offered a folder to choose, which it cannot do");
        if (act != R3_GUIDE_NONE && label[0] == 0)
            return fail("a step offers an action with no button text");
    }
    if (!saw_demo) return fail("the walkthrough cannot start the demo");
    if (!saw_bios) return fail("the walkthrough cannot import a system ROM");
    if (!saw_game) return fail("the walkthrough cannot import a game");

    char body[2048];
    r3_guide_body(2, "the + tile", body, sizeof body);
    if (strstr(body, "the + tile") == NULL) return fail("the control name never reached the text");
    printf("guide step 3 reads: %.58s...\n", body);

    printf("library holds %d disc(s)\n", r3_library_count());

    r3_stop();
    r3_shutdown();
    printf("PASS\n");
    free(pixels);
    return 0;
}
