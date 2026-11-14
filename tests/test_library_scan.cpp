#include "ui/library_scan.h"
#include "test_harness.h"

#include <map>

using namespace retro3do;

TEST(library_scan_finds_games_and_bios_in_one_selected_root) {
    const std::map<std::string, std::vector<DocumentEntry>> folders{
        {"root", {{"games", "content:games", true}, {"BIOS", "content:firmware", true},
                  {"saves", "content:saves", true}, {"nvram.bin", "old:ram", false}}},
        {"content:saves", {{"nvram.bin", "save:ram", false}}},
        {"content:games", {{"Road Rash.CHD", "disc:road", false},
                           {"Alone.cue", "disc:cue", false},
                           {"Alone.bin", "disc:track", false},
                           {"Nested", "content:nested", true}}},
        {"content:nested", {{"Doom.bin", "disc:doom", false}}},
        {"content:firmware", {{"panafz1.bin", "rom:panasonic", false},
                              {"panafz1-kanji.bin", "font:kanji", false},
                              {"Not a disc.iso", "firmware:other", false}}},
    };
    const auto scan = scan_library_folder("root", [&](const std::string& uri) {
        const auto found = folders.find(uri);
        return found == folders.end() ? std::vector<DocumentEntry>{} : found->second;
    }, [](const std::string& uri) { return uri == "rom:panasonic"; });
    CHECK_EQ(scan.discs.size(), size_t{3});
    CHECK(scan.discs[0].uri == "disc:road");
    CHECK(scan.discs[1].uri == "disc:cue");
    CHECK(scan.discs[2].uri == "disc:doom");
    CHECK_EQ(scan.bios.size(), size_t{1});
    CHECK(scan.bios[0].uri == "rom:panasonic");
    CHECK(scan.bios_folder.uri == "content:firmware");
}

TEST(library_scan_checks_bin_bios_before_classifying_discs_or_cue_tracks) {
    for (bool cue : {false, true}) {
        auto scan = scan_library_folder("folder", [&](const std::string&) {
            std::vector<DocumentEntry> files{{"panafz10.bin", "rom", false},
                                             {"Game.bin", "game", false},
                                             {"System.rom", "other-rom", false}};
            if (cue) files.push_back({"Game.CUE", "cue", false});
            return files;
        }, [](const std::string& uri) { return uri == "rom" || uri == "other-rom"; });
        CHECK_EQ(scan.bios.size(), size_t{2});
        CHECK_EQ(scan.discs.size(), size_t{1});
        CHECK(scan.discs[0].uri == (cue ? "cue" : "game"));
    }
}

TEST(library_scan_tolerates_empty_folders_and_directory_cycles) {
    int listings = 0;
    auto list = [&](const std::string&) {
        ++listings;
        return std::vector<DocumentEntry>{{"loop", "root", true}};
    };
    auto scan = scan_library_folder("", list, [](const std::string&) { return false; });
    CHECK_EQ(listings, 0);
    scan = scan_library_folder("root", list, [](const std::string&) { return false; });
    CHECK_EQ(listings, 1);
    CHECK(scan.discs.empty());
    CHECK(scan.bios.empty());
}
