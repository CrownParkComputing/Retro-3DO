// Matching user-supplied cover art to games.
//
// Every case here is a real shape of name from a real folder of disc images.
// The rule this file is guarding is that a wrong cover is worse than none: a
// near miss has to be close, and two different games must never collide.
#include "test_harness.h"

#include "core/covers.h"

using namespace retro3do;

TEST(punctuation_case_and_region_tags_do_not_matter) {
    CHECK(cover_key("Road Rash (USA).chd") == cover_key("road-rash.jpg"));
    CHECK(cover_key("Star Control II [!].iso") == cover_key("Star_Control_II.png"));
    CHECK(cover_key("Gex (USA) (Rev 1).cue") == cover_key("gex.JPG"));
}

TEST(a_leading_article_is_not_part_of_the_name) {
    CHECK(cover_key("The Horde (USA).iso") == cover_key("horde.png"));
    CHECK(cover_key("A Fish Called Wanda.iso") == cover_key("fish called wanda.png"));
}

TEST(only_the_last_extension_is_dropped) {
    // A version in the name survives; the file type does not.
    CHECK(cover_key("Game v1.2.iso") == "gamev12");
}

TEST(an_exact_match_wins) {
    const std::vector<std::string> files = {"roadrashusa.png", "road-rash.jpg", "other.png"};
    CHECK(match_cover("Road Rash.iso", files) == "road-rash.jpg");
}

TEST(a_near_miss_is_accepted_and_the_closest_one_chosen) {
    const std::vector<std::string> files = {"road rash usa europe.png", "road rash usa.png"};
    CHECK(match_cover("Road Rash.iso", files) == "road rash usa.png");
}

TEST(a_different_game_is_never_matched) {
    // The case this whole file exists for. These are different games and a
    // contains-check alone would pair them.
    const std::vector<std::string> files = {"need for speed 2.png"};
    CHECK(match_cover("Need for Speed.iso", files).empty());

    const std::vector<std::string> unrelated = {"shockwave.png"};
    CHECK(match_cover("Road Rash.iso", unrelated).empty());
}

TEST(files_that_are_not_pictures_are_ignored) {
    const std::vector<std::string> files = {"road rash.txt", "road rash.iso", "road rash.nfo"};
    CHECK(match_cover("Road Rash.iso", files).empty());
}

TEST(nothing_matches_nothing) {
    CHECK(match_cover("", {"road rash.png"}).empty());
    CHECK(match_cover("Road Rash.iso", {}).empty());
}
