// A file browser for picking a BIOS or a disc image.
//
// It exists because typing a path is not something a phone user can do, and the
// launcher is otherwise unusable on exactly the platforms this project is for.
// It is deliberately plain: a list of places to start, a list of entries, and a
// way back up. No thumbnails, no search, no favourites — those can come later
// and none of them are what makes the app work.
#pragma once

#include <string>
#include <vector>

namespace retro3do {

class FileBrowser {
public:
    // Open at the most sensible starting place. `extensions` are matched
    // case-insensitively, with the leading dot, and an empty list shows every
    // file.
    // Start beside the remembered file, falling back to a configured folder
    // when the file is unavailable, then to the platform's usual locations.
    // A preferred child folder is used only when opening the configured fallback.
    void open(const std::string& title, std::vector<std::string> extensions,
              const std::string& remembered_file = {},
              const std::string& fallback_folder = {},
              const std::vector<std::string>& preferred_subfolders = {});
    void open_disc(const std::string& title, const std::string& remembered_file,
                   const std::string& library_folder);

    // Open to choose a FOLDER rather than a file. The list still shows files,
    // because seeing the discs in a folder is how anybody knows it is the
    // right one, but they cannot be picked -- a button accepts whichever
    // folder is open instead.
    void open_folder(const std::string& title);
    void close();
    bool is_open() const { return open_; }

    // Returns true on the frame a file was chosen. `chosen_path` gets a
    // filesystem path on desktop, or a document URI on Android — the caller
    // passes either straight to Console, which knows the difference.
    bool draw(std::string* chosen_path, std::string* chosen_name);

private:
    void open_impl(const std::string& title, std::vector<std::string> extensions,
                   const std::string& remembered_file, const std::string& fallback_folder,
                   const std::vector<std::string>& preferred_subfolders);
    bool accepts_entry(const std::string& name, const std::string& target,
                       bool is_directory) const;
    void navigate_to(const std::string& directory);
    void prefer_subfolder(const std::vector<std::string>& names);

    struct Entry {
        std::string name;
        bool is_directory = false;
        // Empty on desktop, where `name` plus the current directory is enough.
        // On Android this is the document URI, because a SAF child cannot be
        // addressed by joining strings — the URI is opaque and must be carried.
        std::string uri;
    };

    // Android hands out document URIs rather than paths, so the browser has two
    // modes. They differ only in how a location is named and listed; everything
    // the user sees is the same.
    bool using_documents_ = false;
    bool choosing_folder_ = false;
    bool disc_images_only_ = false;

    bool open_ = false;
    std::string title_;
    std::string directory_;
    std::vector<std::string> extensions_;
    std::vector<Entry> entries_;
    std::string error_;
};

}  // namespace retro3do
