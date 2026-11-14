#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/types.h"

namespace retro3do {

// One saved console memory shared by all games, with an app-private checkpoint.
class SaveMemory {
public:
    enum class LoadResult { Loaded, Missing, Error };

    SaveMemory(std::size_t bytes, std::string private_directory);
    bool configure(const std::string& library_folder);
    LoadResult load(std::vector<u8>& data) const;
    bool save(const std::vector<u8>& data) const;
    bool has_image() const;
    const std::string& directory() const { return directory_; }

private:
    std::string image_target() const;
    bool read_image(const std::string& target, bool document,
                    std::vector<u8>& data) const;

    std::size_t bytes_;
    std::string private_directory_;
    std::string directory_;
    bool documents_ = false;
};

}  // namespace retro3do
