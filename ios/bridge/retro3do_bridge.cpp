#include "retro3do_bridge.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "console.h"
#include "frame_mailbox.h"
#include "game_library.h"
#include "guide.h"

using namespace retro3do;

namespace {

// A 3DO system ROM is a one-megabyte window, so nothing legitimate is larger
// and every disc image is far bigger. Size narrows it down; it does not decide.
constexpr long kMaxBiosBytes = 4L * 1024 * 1024;

// Size alone is not enough, and assuming it was is a trap worth naming:
// Console::load_bios accepts any file that fits the ROM window — it has no
// opinion about the contents — so classifying by size would greet a user who
// picked the wrong small file with "System ROM added", and then fail every
// disc afterwards for reasons they could not possibly guess.
//
// The ARM60 boots from the bottom of ROM into the standard ARM exception
// vector table: eight words, each a branch. Real BIOS images and this
// project's own demo ROM alike begin 0xEA — ARM's unconditional B, big-endian.
// A file that does not is not something this machine could ever execute.
// The same trap on the disc side: Disc is a sector source and Console::load_disc
// asks it only to open, so a 2 KB file of nothing opens happily and would have
// appeared on the shelf as a game. A 3DO disc says what it is in its first
// sector — record type 1 followed by the five sync bytes "ZZZZZ" at the top of
// the Opera volume header — and reading it through Disc::read_sector means
// .iso, .bin/.cue, .img and .chd are all checked the same way, whatever their
// sector layout.
constexpr u32 kVolumeHeaderLabelOffset = 40;
constexpr u32 kVolumeHeaderLabelBytes  = 32;

bool is_a_3do_disc(Disc& disc, std::string* out_label) {
    u8 sector[kSectorUserBytes] = {0};
    if (!disc.read_sector(0, sector)) return false;
    if (sector[0] != 0x01) return false;
    if (std::memcmp(sector + 1, "ZZZZZ", 5) != 0) return false;

    // The volume label is the disc's own name for itself, which beats a file
    // name somebody's dumping tool chose. Trusted only as far as it is
    // printable: it is bytes off a disc image, and it is going on a tile.
    if (out_label != nullptr) {
        std::string label;
        for (u32 i = 0; i < kVolumeHeaderLabelBytes; ++i) {
            const u8 c = sector[kVolumeHeaderLabelOffset + i];
            if (c == 0) break;
            if (c < 0x20 || c > 0x7E) { label.clear(); break; }
            label.push_back(static_cast<char>(c));
        }
        while (!label.empty() && label.back() == ' ') label.pop_back();
        *out_label = label;
    }
    return true;
}

bool looks_like_a_system_rom(const unsigned char* head, size_t len) {
    if (len < 32) return false;
    if (head[0] != 0xEA) return false;          // the reset vector must branch
    int vectors = 0;
    for (size_t word = 0; word < 8; ++word) {
        const unsigned char top = head[word * 4];
        // B, or the LDR/LDM forms some ROMs use to jump further than a branch
        // can reach.
        if (top == 0xEA || top == 0xE5 || top == 0xE9) ++vectors;
    }
    return vectors >= 4;
}

struct Bridge {
    std::unique_ptr<Console> console;
    FrameMailbox mailbox;

    std::thread thread;
    std::atomic<bool> quit{false};
    std::atomic<bool> paused{false};
    std::atomic<bool> playing{false};

    // Held for a whole emulated frame by the producer, and by any call that
    // touches the Console. A caller may wait up to one frame; a person cannot
    // perceive that, and it buys a model with no command queue to get wrong.
    std::mutex console_mutex;

    // Taken by the producer only when the picture changes size, and by the
    // consumer for the length of a copy. Uncontended in the steady state, and
    // the only thing standing between a region change and a torn read.
    std::mutex resize_mutex;

    std::atomic<int> fb_w{0};
    std::atomic<int> fb_h{0};
    std::atomic<bool> audio_live{false};
    std::atomic<int> fps_x100{0};
    std::atomic<bool> pal{false};

    std::string support_dir;
    std::string error;
    std::mutex error_mutex;

    struct Entry { std::string name, path; };
    std::vector<Entry> library;
    std::mutex library_mutex;
};

Bridge* g = nullptr;

void set_error(const std::string& text) {
    if (g == nullptr) return;
    std::lock_guard<std::mutex> lock(g->error_mutex);
    g->error = text;
}

std::string join(const std::string& dir, const char* leaf) {
    if (dir.empty()) return leaf;
    if (dir.back() == '/') return dir + leaf;
    return dir + "/" + leaf;
}

int copy_out(const std::string& text, char* out, int out_len) {
    if (out == nullptr || out_len <= 0) return static_cast<int>(text.size());
    const int n = static_cast<int>(
        text.size() < static_cast<size_t>(out_len - 1) ? text.size() : out_len - 1);
    std::memcpy(out, text.data(), static_cast<size_t>(n));
    out[n] = '\0';
    return n;
}

// ------------------------------------------------------------------- NVRAM --
// The 3DO keeps game saves in battery-backed memory. It is one small file and
// it is written on every stop rather than on a timer, so that iOS killing the
// app in the background cannot cost a save.

void restore_nvram() {
    const std::string path = join(g->support_dir, "nvram.bin");
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return;   // first run: the machine has already formatted one
    std::vector<u8> data(g->console->bus().nvram_size());
    const size_t read = std::fread(data.data(), 1, data.size(), f);
    std::fclose(f);
    if (read == data.size()) g->console->bus().restore_nvram(data.data(), data.size());
}

void flush_nvram() {
    if (g->console == nullptr) return;
    if (!g->console->bus().nvram_dirty()) return;
    const std::string path = join(g->support_dir, "nvram.bin");
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) return;
    std::fwrite(g->console->bus().nvram(), 1, g->console->bus().nvram_size(), f);
    std::fclose(f);
    g->console->bus().clear_nvram_dirty();
}

// ----------------------------------------------------------------- library --
// A two-column text file beside the NVRAM. Not a database: the list is a few
// dozen lines at most, and a format a person can read is a format that can be
// repaired by hand if it is ever damaged.

void library_load() {
    const std::string path = join(g->support_dir, "library.txt");
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return;
    std::string line;
    int c;
    auto commit = [&]() {
        const size_t tab = line.find('\t');
        if (tab != std::string::npos && tab + 1 < line.size()) {
            g->library.push_back({line.substr(0, tab), line.substr(tab + 1)});
        }
        line.clear();
    };
    while ((c = std::fgetc(f)) != EOF) {
        if (c == '\n') commit();
        else if (c != '\r') line.push_back(static_cast<char>(c));
    }
    if (!line.empty()) commit();
    std::fclose(f);
}

void library_save() {
    const std::string path = join(g->support_dir, "library.txt");
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) return;
    for (const Bridge::Entry& e : g->library) {
        std::fprintf(f, "%s\t%s\n", e.name.c_str(), e.path.c_str());
    }
    std::fclose(f);
}

// ------------------------------------------------------------ emulation --

void emulate() {
    using clock = std::chrono::steady_clock;
    auto next = clock::now();
    auto fps_mark = next;
    int frames_since_mark = 0;

    while (!g->quit.load(std::memory_order_relaxed)) {
        if (!g->playing.load(std::memory_order_relaxed) ||
            g->paused.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
            next = clock::now();
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(g->console_mutex);
            g->console->run_frame();
            const Frame frame = g->console->framebuffer();
            if (frame.pixels != nullptr && frame.width > 0 && frame.height > 0) {
                if (g->mailbox.width() != frame.width ||
                    g->mailbox.height() != frame.height) {
                    std::lock_guard<std::mutex> resize(g->resize_mutex);
                    g->mailbox.resize(frame.width, frame.height);
                    g->fb_w.store(frame.width, std::memory_order_relaxed);
                    g->fb_h.store(frame.height, std::memory_order_relaxed);
                }
                std::memcpy(g->mailbox.writable(), frame.pixels,
                            static_cast<size_t>(frame.width) *
                                static_cast<size_t>(frame.height) * sizeof(u32));
                g->mailbox.publish();
            }
        }

        ++frames_since_mark;
        const auto now = clock::now();
        if (now - fps_mark >= std::chrono::seconds(1)) {
            const double seconds =
                std::chrono::duration<double>(now - fps_mark).count();
            g->fps_x100.store(static_cast<int>((frames_since_mark / seconds) * 100.0),
                              std::memory_order_relaxed);
            frames_since_mark = 0;
            fps_mark = now;
        }

        // Paced on the wall clock rather than on the audio ring. The ring holds
        // 740 ms, far more than the drift one second of scheduling jitter can
        // produce, and pacing on it would make a stalled audio unit stop
        // emulation — which is how a background app loses a frame of input.
        const auto period = g->pal.load(std::memory_order_relaxed)
                                ? std::chrono::nanoseconds(20000000)    // 50 Hz
                                : std::chrono::nanoseconds(16683333);   // 60 Hz
        next += period;
        if (next < now) {
            // More than a frame behind: give up the lost time rather than
            // sprinting to catch up, which would play the game fast.
            next = now;
        } else {
            std::this_thread::sleep_until(next);
        }
    }
}

}  // namespace

// ================================================================= lifecycle ==

bool r3_start(const char* support_dir) {
    if (g != nullptr) return true;
    g = new Bridge();
    g->support_dir = support_dir != nullptr ? support_dir : "";
    g->console = std::make_unique<Console>();
    if (g->console == nullptr) {
        set_error("The machine could not be created.");
        return false;
    }
    restore_nvram();
    library_load();
    g->audio_live.store(true, std::memory_order_release);
    g->thread = std::thread(emulate);
    return true;
}

void r3_shutdown(void) {
    if (g == nullptr) return;
    g->audio_live.store(false, std::memory_order_release);
    g->quit.store(true, std::memory_order_relaxed);
    if (g->thread.joinable()) g->thread.join();
    {
        std::lock_guard<std::mutex> lock(g->console_mutex);
        flush_nvram();
    }
    delete g;
    g = nullptr;
}

// =================================================================== content ==

R3ImportKind r3_import(const char* path, char* out_name, int out_name_len) {
    if (g == nullptr || path == nullptr) return R3_IMPORT_FAILED;

    std::FILE* probe = std::fopen(path, "rb");
    if (probe == nullptr) {
        set_error("That file could not be opened.");
        return R3_IMPORT_FAILED;
    }
    unsigned char head[32] = {0};
    const size_t head_len = std::fread(head, 1, sizeof head, probe);
    std::fseek(probe, 0, SEEK_END);
    const long size = std::ftell(probe);
    std::fclose(probe);
    if (size <= 0) {
        set_error("That file is empty.");
        return R3_IMPORT_FAILED;
    }

    const std::string full = path;
    const size_t slash = full.find_last_of('/');
    const std::string leaf = slash == std::string::npos ? full : full.substr(slash + 1);
    const std::string nice = GameLibrary::display_name(leaf);

    std::lock_guard<std::mutex> lock(g->console_mutex);

    // Small enough and shaped like one: try it as a system ROM. Succeeding both
    // tells us what the file is and leaves it loaded, so the user never sees
    // the word BIOS unless they are missing one.
    if (size <= kMaxBiosBytes && looks_like_a_system_rom(head, head_len) &&
        g->console->load_bios(full)) {
        copy_out(nice, out_name, out_name_len);
        return R3_IMPORT_BIOS;
    }

    // Checked on a Disc of our own rather than the machine's. Importing should
    // not disturb what is in the drive, and the Console only lends its disc out
    // as const — rightly, since nothing outside it should be moving the head.
    Disc candidate;
    std::string label;
    const bool genuine = candidate.open(full) && is_a_3do_disc(candidate, &label);
    candidate.close();
    if (!genuine) {
        // Said in terms of what the person did, not what the loader found: by
        // this point it is neither a 3DO disc nor anything the ARM60 could boot.
        set_error("That is not a 3DO disc or system ROM.");
        return R3_IMPORT_FAILED;
    }

    const std::string title = label.empty() ? nice : label;
    {
        std::lock_guard<std::mutex> lib(g->library_mutex);
        bool known = false;
        for (const Bridge::Entry& e : g->library) known = known || e.path == full;
        if (!known) g->library.push_back({title, full});
        library_save();
    }
    copy_out(title, out_name, out_name_len);
    return R3_IMPORT_DISC;
}

bool r3_has_bios(void) {
    if (g == nullptr) return false;
    std::lock_guard<std::mutex> lock(g->console_mutex);
    return g->console->bios_loaded() && !g->console->demo_loaded();
}

bool r3_play_demo(void) {
    if (g == nullptr) return false;
    std::lock_guard<std::mutex> lock(g->console_mutex);
    g->console->eject_disc();
    if (!g->console->load_builtin_demo()) {
        set_error(g->console->last_error());
        return false;
    }
    g->console->reset();
    g->paused.store(false, std::memory_order_relaxed);
    g->playing.store(true, std::memory_order_relaxed);
    return true;
}

bool r3_play_disc(const char* path) {
    if (g == nullptr || path == nullptr) return false;
    std::lock_guard<std::mutex> lock(g->console_mutex);
    if (!g->console->bios_loaded() || g->console->demo_loaded()) {
        set_error("This disc needs a 3DO system ROM. Add yours with the + tile.");
        return false;
    }
    if (!g->console->load_disc(path)) {
        set_error(g->console->last_error());
        return false;
    }
    g->console->reset();
    g->paused.store(false, std::memory_order_relaxed);
    g->playing.store(true, std::memory_order_relaxed);
    return true;
}

void r3_stop(void) {
    if (g == nullptr) return;
    g->playing.store(false, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(g->console_mutex);
    flush_nvram();
}

bool r3_is_playing(void) {
    return g != nullptr && g->playing.load(std::memory_order_relaxed);
}

void r3_set_paused(bool paused) {
    if (g == nullptr) return;
    g->paused.store(paused, std::memory_order_relaxed);
    if (paused) {
        std::lock_guard<std::mutex> lock(g->console_mutex);
        flush_nvram();
    }
}

bool r3_is_paused(void) {
    return g != nullptr && g->paused.load(std::memory_order_relaxed);
}

void r3_reset(void) {
    if (g == nullptr) return;
    std::lock_guard<std::mutex> lock(g->console_mutex);
    g->console->reset();
}

// =================================================================== library ==

int r3_library_count(void) {
    if (g == nullptr) return 0;
    std::lock_guard<std::mutex> lock(g->library_mutex);
    return static_cast<int>(g->library.size());
}

int r3_library_name(int index, char* out, int out_len) {
    if (g == nullptr) return 0;
    std::lock_guard<std::mutex> lock(g->library_mutex);
    if (index < 0 || index >= static_cast<int>(g->library.size())) return 0;
    return copy_out(g->library[static_cast<size_t>(index)].name, out, out_len);
}

int r3_library_path(int index, char* out, int out_len) {
    if (g == nullptr) return 0;
    std::lock_guard<std::mutex> lock(g->library_mutex);
    if (index < 0 || index >= static_cast<int>(g->library.size())) return 0;
    return copy_out(g->library[static_cast<size_t>(index)].path, out, out_len);
}

void r3_library_forget(int index) {
    if (g == nullptr) return;
    std::lock_guard<std::mutex> lock(g->library_mutex);
    if (index < 0 || index >= static_cast<int>(g->library.size())) return;
    g->library.erase(g->library.begin() + index);
    library_save();
}

// =================================================================== picture ==

uint64_t r3_frame_serial(void) {
    if (g == nullptr) return 0;
    return g->mailbox.published();
}

int r3_copy_frame(uint32_t* dst, int capacity, int* out_w, int* out_h) {
    if (g == nullptr) return 0;
    std::lock_guard<std::mutex> lock(g->resize_mutex);
    const int w = g->fb_w.load(std::memory_order_relaxed);
    const int h = g->fb_h.load(std::memory_order_relaxed);
    if (out_w != nullptr) *out_w = w;
    if (out_h != nullptr) *out_h = h;
    if (w <= 0 || h <= 0) return 0;
    const int pixels = w * h;
    if (dst == nullptr || capacity < pixels) return 0;

    const u32* src = g->mailbox.acquire();
    if (src == nullptr) src = g->mailbox.current();
    if (src == nullptr) return 0;
    std::memcpy(dst, src, static_cast<size_t>(pixels) * sizeof(u32));
    return pixels;
}

int r3_pixel_aspect_x1000(void) {
    if (g == nullptr) return 1000;
    // Both regions put the same 4:3 picture on the screen; PAL simply uses more
    // lines for it, so its pixels are shorter rather than the picture taller.
    const int w = g->fb_w.load(std::memory_order_relaxed);
    const int h = g->fb_h.load(std::memory_order_relaxed);
    if (w <= 0 || h <= 0) return 1000;
    return static_cast<int>((4.0 / 3.0) * h * 1000.0 / w);
}

// ===================================================================== sound ==

int r3_pull_audio(int16_t* dst, int frames) {
    if (dst == nullptr || frames <= 0) return 0;
    if (g == nullptr || !g->audio_live.load(std::memory_order_acquire)) {
        std::memset(dst, 0, static_cast<size_t>(frames) * 2 * sizeof(int16_t));
        return 0;
    }
    // AudioRing is a lock-free SPSC ring, so the audio unit's realtime thread
    // takes no lock here and cannot be delayed by an emulated frame.
    const u32 real = g->console->audio().pull(
        reinterpret_cast<StereoSample*>(dst), static_cast<u32>(frames));
    return static_cast<int>(real);
}

// ===================================================================== input ==

void r3_pad(uint32_t mask) {
    if (g == nullptr) return;
    Joypad pad;
    pad.up          = (mask & R3_PAD_UP) != 0;
    pad.down        = (mask & R3_PAD_DOWN) != 0;
    pad.left        = (mask & R3_PAD_LEFT) != 0;
    pad.right       = (mask & R3_PAD_RIGHT) != 0;
    pad.a           = (mask & R3_PAD_A) != 0;
    pad.b           = (mask & R3_PAD_B) != 0;
    pad.c           = (mask & R3_PAD_C) != 0;
    pad.play        = (mask & R3_PAD_PLAY) != 0;
    pad.stop        = (mask & R3_PAD_STOP) != 0;
    pad.left_shift  = (mask & R3_PAD_LEFT_SHIFT) != 0;
    pad.right_shift = (mask & R3_PAD_RIGHT_SHIFT) != 0;
    std::lock_guard<std::mutex> lock(g->console_mutex);
    g->console->set_joypad(pad);
}

// ================================================================== settings ==

void r3_set_region(bool pal) {
    if (g == nullptr) return;
    std::lock_guard<std::mutex> lock(g->console_mutex);
    g->pal.store(pal, std::memory_order_relaxed);
    g->console->set_region(pal ? Region::Pal : Region::Ntsc);
}

bool r3_region_is_pal(void) {
    return g != nullptr && g->pal.load(std::memory_order_relaxed);
}

// =============================================================== diagnostics ==

const char* r3_last_error(void) {
    static std::string copy;
    if (g == nullptr) return "";
    std::lock_guard<std::mutex> lock(g->error_mutex);
    copy = g->error;
    return copy.c_str();
}

int r3_fps_x100(void) {
    if (g == nullptr) return 0;
    return g->fps_x100.load(std::memory_order_relaxed);
}

// ===================================================================== guide ==
// No Bridge state: the steps are compiled-in constants, so these work before
// r3_start() and cannot fail. Only the iOS steps are ever visible here --
// pointing at a folder is not a thing iOS lets an app do, so the Android and
// desktop versions of that step are filtered out before Swift sees them.

int r3_guide_count(void) {
    return static_cast<int>(guide_step_count(kGuideIos));
}

int r3_guide_title(int step, char* out, int out_len) {
    const GuideStep* s = guide_step(kGuideIos, static_cast<std::size_t>(step));
    if (step < 0 || s == nullptr) return 0;
    return copy_out(s->title, out, out_len);
}

int r3_guide_body(int step, const char* add_control, char* out, int out_len) {
    const GuideStep* s = guide_step(kGuideIos, static_cast<std::size_t>(step));
    if (step < 0 || s == nullptr || out == nullptr || out_len <= 0) return 0;
    guide_body(s->body, add_control, out, static_cast<std::size_t>(out_len));
    return static_cast<int>(std::strlen(out));
}

uint32_t r3_guide_accent(int step) {
    const GuideStep* s = guide_step(kGuideIos, static_cast<std::size_t>(step));
    return (step < 0 || s == nullptr) ? 0xFD9E3Du : s->accent;
}

int r3_guide_action(int step, int slot) {
    const GuideStep* s = guide_step(kGuideIos, static_cast<std::size_t>(step));
    if (step < 0 || s == nullptr) return R3_GUIDE_NONE;
    return static_cast<int>(slot == 0 ? s->action : s->action2);
}

int r3_guide_action_label(int step, int slot, char* out, int out_len) {
    const GuideStep* s = guide_step(kGuideIos, static_cast<std::size_t>(step));
    if (step < 0 || s == nullptr) return 0;
    const char* label = slot == 0 ? s->action_label : s->action2_label;
    return label == nullptr ? 0 : copy_out(label, out, out_len);
}
