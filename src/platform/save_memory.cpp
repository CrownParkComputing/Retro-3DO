#include "save_memory.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <utility>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#include "platform/android_storage.h"
#include "platform/storage.h"

namespace retro3do {
namespace {

constexpr const char* kImage = "nvram.bin";

std::FILE* file_from_fd(int fd, const char* mode) {
#if defined(_WIN32)
    std::FILE* file = ::_fdopen(fd, mode);
    if (!file) ::_close(fd);
#else
    std::FILE* file = ::fdopen(fd, mode);
    if (!file) ::close(fd);
#endif
    return file;
}

bool write_atomic(const std::string& path, const std::vector<u8>& data) {
    const std::string temporary = path + ".tmp";
    if (SDL_SaveFile(temporary.c_str(), data.data(), data.size()) &&
        SDL_RenamePath(temporary.c_str(), path.c_str())) return true;
    SDL_RemovePath(temporary.c_str());
    return false;
}

}  // namespace

SaveMemory::SaveMemory(std::size_t bytes, std::string private_directory)
    : bytes_(bytes), private_directory_(std::move(private_directory)) {}

bool SaveMemory::configure(const std::string& library_folder) {
    documents_ = AndroidStorage::available() && !library_folder.empty();
    if (documents_) {
        directory_ = AndroidStorage::ensure_directory(library_folder, "saves");
        if (!directory_.empty()) return true;
        documents_ = false;
    } else if (!library_folder.empty()) {
        std::string root = library_folder;
        std::string name = Storage::base_name(root);
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (name == "games" || name == "discs") root = Storage::parent_of(root);
        directory_ = Storage::join(root, "saves");
        if (SDL_CreateDirectory(directory_.c_str())) return true;
    }
    directory_ = Storage::join(private_directory_, "saves");
    return SDL_CreateDirectory(directory_.c_str()) && library_folder.empty();
}

std::string SaveMemory::image_target() const {
    if (!documents_) return Storage::join(directory_, kImage);
    for (const auto& entry : AndroidStorage::list(directory_)) {
        if (!entry.is_directory && entry.name == kImage) return entry.uri;
    }
    return {};
}

bool SaveMemory::has_image() const {
    const std::string target = image_target();
    return !target.empty() && (documents_ || Storage::exists(target));
}

bool SaveMemory::read_image(const std::string& target, bool document,
                             std::vector<u8>& data) const {
    if (target.empty()) return false;
    std::FILE* file = nullptr;
    if (document) {
        const int fd = AndroidStorage::open_document(target);
        if (fd < 0) return false;
        file = file_from_fd(fd, "rb");
    } else {
        file = std::fopen(target.c_str(), "rb");
    }
    if (!file) return false;
    std::vector<u8> image(bytes_ + 1);
    const size_t count = std::fread(image.data(), 1, image.size(), file);
    const bool valid = count == bytes_ && !std::ferror(file);
    std::fclose(file);
    if (!valid) return false;
    image.resize(bytes_);
    data = std::move(image);
    return true;
}

SaveMemory::LoadResult SaveMemory::load(std::vector<u8>& data) const {
    const std::string target = image_target();
    if (read_image(target, documents_, data)) return LoadResult::Loaded;
    const std::string checkpoint = Storage::join(private_directory_, kImage);
    if (read_image(checkpoint, false, data)) return LoadResult::Loaded;
    return has_image() || Storage::exists(checkpoint)
        ? LoadResult::Error : LoadResult::Missing;
}

bool SaveMemory::save(const std::vector<u8>& data) const {
    if (data.size() != bytes_) return false;
    // Keep the existing private file as a recovery checkpoint as well as the
    // migration source. A failed removable-storage write cannot lose the RAM.
    const bool checkpoint = write_atomic(Storage::join(private_directory_, kImage), data);
    bool saved = false;
    if (documents_) {
        const int fd = AndroidStorage::open_write_document(directory_, kImage);
        if (fd >= 0) {
            std::FILE* file = file_from_fd(fd, "wb");
            if (file) {
                saved = std::fwrite(data.data(), 1, data.size(), file) == data.size();
                saved = std::fclose(file) == 0 && saved;
            }
        }
    } else {
        saved = write_atomic(image_target(), data);
    }
    return saved && checkpoint;
}

}  // namespace retro3do
