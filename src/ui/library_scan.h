#pragma once

#include <functional>
#include <string>
#include <vector>

#include "platform/android_storage.h"

namespace retro3do {

struct LibraryScan {
    std::vector<DocumentEntry> discs;
    std::vector<DocumentEntry> bios;
    DocumentEntry bios_folder;
};

// Targets stay opaque: SAF child URIs must come from the listing, never from
// joining a folder URI and a filename. The same scan serves desktop paths.
LibraryScan scan_library_folder(
    const std::string& target,
    const std::function<std::vector<DocumentEntry>(const std::string&)>& list,
    const std::function<bool(const std::string&)>& is_system_rom);

}  // namespace retro3do
