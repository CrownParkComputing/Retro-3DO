#include "platform/save_memory.h"
#include "platform/storage.h"
#include "platform/emulator_thread.h"
#include "core/console.h"
#include "core/frame_mailbox.h"
#include "test_harness.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace retro3do;
namespace fs = std::filesystem;

namespace {

struct Folders {
    fs::path root;
    fs::path library;
    fs::path private_dir;
    Folders() {
        static unsigned counter = 0;
        root = fs::temp_directory_path() / ("retro3do-save-test-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
            "-" + std::to_string(counter++));
        library = root / "library";
        private_dir = root / "private";
        fs::create_directories(library);
        fs::create_directories(private_dir);
    }
    ~Folders() { std::error_code error; fs::remove_all(root, error); }
};

void put(const fs::path& path, const std::vector<u8>& data) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
}

std::vector<u8> get(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<u8>(std::istreambuf_iterator<char>(file), {});
}

constexpr size_t kBytes = 32 * 1024;

}  // namespace

TEST(save_memory_migrates_existing_ram_and_survives_a_restart) {
    Folders folders;
    const std::vector<u8> original(kBytes, 0xA5);
    put(folders.private_dir / "nvram.bin", original);
    SaveMemory store(kBytes, folders.private_dir.string());
    CHECK(store.configure(folders.library.string()));
    CHECK(fs::is_directory(folders.library / "saves"));
    std::vector<u8> loaded;
    CHECK(store.load(loaded) == SaveMemory::LoadResult::Loaded);
    CHECK(loaded == original);
    CHECK(!store.has_image());
    CHECK(store.save(loaded));
    CHECK(get(folders.private_dir / "nvram.bin") == original);
    SaveMemory restarted(kBytes, folders.private_dir.string());
    CHECK(restarted.configure(folders.library.string()));
    CHECK(restarted.load(loaded) == SaveMemory::LoadResult::Loaded);
    CHECK(loaded == original);
}

TEST(save_memory_prefers_the_library_save_to_an_older_private_copy) {
    Folders folders;
    SaveMemory store(kBytes, folders.private_dir.string());
    CHECK(store.configure(folders.library.string()));
    const std::vector<u8> current(kBytes, 0x42);
    const std::vector<u8> old(kBytes, 0x11);
    CHECK(store.save(current));
    put(folders.private_dir / "nvram.bin", old);
    std::vector<u8> loaded;
    CHECK(store.load(loaded) == SaveMemory::LoadResult::Loaded);
    CHECK(loaded == current);
    CHECK(!store.save(std::vector<u8>(kBytes - 1, 0)));
    CHECK(get(folders.library / "saves/nvram.bin") == current);
    CHECK(get(folders.private_dir / "nvram.bin") == old);
}

TEST(save_memory_keeps_a_checkpoint_when_external_saving_fails_then_retries) {
    Folders folders;
    SaveMemory store(kBytes, folders.private_dir.string());
    CHECK(store.configure(folders.library.string()));
    CHECK(store.save(std::vector<u8>(kBytes, 1)));
    fs::remove_all(folders.library / "saves");
    put(folders.library / "saves", {0}); // a file blocks the destination directory
    const std::vector<u8> latest(kBytes, 0x7E);
    CHECK(!store.save(latest));
    std::vector<u8> loaded;
    CHECK(store.load(loaded) == SaveMemory::LoadResult::Loaded);
    CHECK(loaded == latest);
    fs::remove(folders.library / "saves");
    fs::create_directory(folders.library / "saves");
    CHECK(store.save(latest));
    CHECK(get(folders.library / "saves/nvram.bin") == latest);
}

TEST(save_memory_leaves_invalid_existing_data_untouched) {
    Folders folders;
    SaveMemory store(kBytes, folders.private_dir.string());
    CHECK(store.configure(folders.library.string()));
    std::vector<u8> loaded{0x66};
    CHECK(store.load(loaded) == SaveMemory::LoadResult::Missing);
    put(folders.library / "saves/nvram.bin", {1, 2, 3});
    CHECK(store.load(loaded) == SaveMemory::LoadResult::Error);
    CHECK(loaded == std::vector<u8>{0x66});
    CHECK(get(folders.library / "saves/nvram.bin") == (std::vector<u8>{1, 2, 3}));
}

TEST(save_memory_places_saves_beside_a_selected_games_folder) {
    Folders folders;
    fs::create_directory(folders.library / "games");
    SaveMemory store(kBytes, folders.private_dir.string());
    CHECK(store.configure((folders.library / "games").string()));
    CHECK(store.directory() == (folders.library / "saves").string());
    CHECK(!fs::exists(folders.library / "games/saves"));
}

TEST(save_ram_restore_and_snapshot_are_safe_during_emulation) {
    Console console;
    CHECK(console.load_builtin_demo());
    FrameMailbox mailbox;
    EmulatorThread emulator(console, mailbox);
    emulator.set_paused(true);
    emulator.start();
    std::vector<u8> expected;
    emulator.snapshot_nvram(expected);
    expected[300] = 0xA5;
    CHECK(emulator.restore_nvram(expected));
    std::vector<u8> actual;
    emulator.snapshot_nvram(actual);
    CHECK(actual == expected);
    // A game writes through the bus. Its next completed field must publish
    // that RAM for storage, rather than only saving our explicit restores.
    console.bus().write32(kNvramBase + 302 * kNvramStride, 0x33);
    expected[302] = 0x33;
    emulator.set_paused(false);
    for (int i = 0; i < 2000 && emulator.stats().frames_emulated < 2; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(emulator.stats().frames_emulated >= 2);
    CHECK(emulator.take_nvram(actual));
    CHECK(actual == expected);
    Folders folders;
    SaveMemory store(kBytes, folders.private_dir.string());
    CHECK(store.configure(folders.library.string()));
    CHECK(store.save(actual));
    actual.clear();
    CHECK(store.load(actual) == SaveMemory::LoadResult::Loaded);
    CHECK(actual == expected);
    expected[301] = 0x5A;
    CHECK(emulator.restore_nvram(expected));
    emulator.snapshot_nvram(actual);
    CHECK(actual == expected);
    CHECK(!emulator.restore_nvram({}));
    emulator.stop();
}
