#include "library_scan.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace retro3do {
namespace {

std::string lowercased(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::string extension(const std::string& name) {
    const size_t dot = name.find_last_of('.');
    return dot == std::string::npos ? "" : lowercased(name.substr(dot));
}

}  // namespace

LibraryScan scan_library_folder(
    const std::string& target,
    const std::function<std::vector<DocumentEntry>(const std::string&)>& list,
    const std::function<bool(const std::string&)>& is_system_rom) {
    LibraryScan result;
    if (target.empty()) return result;
    struct Folder { std::string target; int depth; bool bios_only; };
    std::vector<Folder> pending{{target, 0, false}};
    std::set<std::string> visited{target};
    for (size_t cursor = 0; cursor < pending.size() && cursor < 1024; ++cursor) {
        const auto [folder, depth, bios_only] = pending[cursor];
        const auto entries = list(folder);
        const bool has_cue = std::any_of(entries.begin(), entries.end(),
            [](const DocumentEntry& entry) {
                return !entry.is_directory && extension(entry.name) == ".cue";
            });
        for (const DocumentEntry& entry : entries) {
            if (entry.is_directory) {
                if (lowercased(entry.name) == "saves") continue;
                const bool is_bios = lowercased(entry.name) == "bios";
                if (is_bios && result.bios_folder.uri.empty()) {
                    result.bios_folder = entry;
                }
                if (depth < 8 && pending.size() < 1024 &&
                    visited.insert(entry.uri).second) {
                    pending.push_back({entry.uri, depth + 1, bios_only || is_bios});
                }
                continue;
            }
            if (lowercased(entry.name) == "nvram.bin") continue;
            const std::string ext = extension(entry.name);
            const bool disc = ext == ".chd" || ext == ".iso" || ext == ".bin" ||
                              ext == ".cue" || ext == ".img";
            // BIN is ambiguous. Check ROM contents before treating it as a
            // disc or suppressing it as a CUE track. Other disc formats need
            // no probing; an unknown extension may still hold a system ROM.
            if ((!disc || ext == ".bin") && is_system_rom(entry.uri)) {
                result.bios.push_back(entry);
            } else if (!bios_only && disc && !(has_cue && ext == ".bin") &&
                       result.discs.size() < 2048) {
                result.discs.push_back(entry);
            }
        }
    }
    return result;
}

}  // namespace retro3do
