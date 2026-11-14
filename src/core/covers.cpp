#include "covers.h"

#include <algorithm>
#include <cctype>

namespace retro3do {
namespace {

bool is_separator(unsigned char c) {
    return c == ' ' || c == '_' || c == '-' || c == '.' || c == ',' ||
           c == '\'' || c == '!' || c == '&' || c == '+';
}

}  // namespace

const std::vector<std::string>& cover_extensions() {
    static const std::vector<std::string> kExtensions = {".png", ".jpg", ".jpeg", ".webp"};
    return kExtensions;
}

std::string cover_key(const std::string& name) {
    std::string out;
    out.reserve(name.size());

    int depth = 0;           // inside (brackets) or [brackets]
    bool seen_dot = false;
    // Drop a trailing extension, but only the last one, so "Game v1.2.iso"
    // keeps its version and loses the iso.
    const size_t last_dot = name.find_last_of('.');
    const size_t end = last_dot == std::string::npos || last_dot == 0
                           ? name.size()
                           : last_dot;
    (void)seen_dot;

    for (size_t i = 0; i < end; ++i) {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        if (c == '(' || c == '[') { ++depth; continue; }
        if (c == ')' || c == ']') { if (depth > 0) --depth; continue; }
        if (depth > 0) continue;                 // region and version tags
        if (std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
        } else if (is_separator(c)) {
            continue;                            // punctuation carries no meaning here
        }
    }

    // A leading article is the one word people disagree about: "The Horde" on
    // the disc and "horde.png" in the folder is the same game.
    static const char* kArticles[] = {"the", "a", "an"};
    for (const char* article : kArticles) {
        const size_t n = std::string(article).size();
        if (out.size() > n + 2 && out.compare(0, n, article) == 0) {
            out.erase(0, n);
            break;
        }
    }
    return out;
}

std::string match_cover(const std::string& game_name,
                        const std::vector<std::string>& files) {
    const std::string wanted = cover_key(game_name);
    if (wanted.empty()) return {};

    std::string best;
    size_t best_extra = 0;
    for (const std::string& file : files) {
        const size_t dot = file.find_last_of('.');
        if (dot == std::string::npos) continue;
        std::string ext = file.substr(dot);
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        const std::vector<std::string>& accepted = cover_extensions();
        if (std::find(accepted.begin(), accepted.end(), ext) == accepted.end()) continue;

        const std::string key = cover_key(file);
        if (key.empty()) continue;

        if (key == wanted) return file;          // exact: nothing beats it

        // One containing the other is the usual near miss -- "roadrash" against
        // "roadrashusa". Accept it, preferring the closest, but never when the
        // difference is large enough that it could be a different game: "nfs"
        // must not match "nfs2".
        const bool contains = key.find(wanted) != std::string::npos ||
                              wanted.find(key) != std::string::npos;
        if (!contains) continue;
        const std::string& longer = key.size() > wanted.size() ? key : wanted;
        const std::string& shorter = key.size() > wanted.size() ? wanted : key;
        const size_t extra = longer.size() - shorter.size();
        if (extra > 6) continue;

        // What the longer name has that the shorter does not. A digit in there
        // means a sequel -- "needforspeed" against "needforspeed2" is one
        // character apart and a different game -- while letters are a region
        // or an edition and are fine to ignore.
        std::string difference = longer;
        const size_t at = difference.find(shorter);
        if (at != std::string::npos) difference.erase(at, shorter.size());
        if (std::any_of(difference.begin(), difference.end(), [](unsigned char c) {
                return std::isdigit(c) != 0;
            })) {
            continue;
        }
        if (best.empty() || extra < best_extra) {
            best = file;
            best_extra = extra;
        }
    }
    return best;
}

}  // namespace retro3do
