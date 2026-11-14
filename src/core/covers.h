// Matching cover art a user has supplied to the games the app can see.
//
// There is no artwork service behind this and there deliberately is not: the
// one this project had was taken out, and the alternative needs a credential
// shipped inside the app, which is not a credential. A folder of pictures the
// user owns costs nothing, works offline, and cannot stop working.
//
// The whole problem is that the two names never match exactly. A disc might be
// "Road Rash (USA)" and the picture "road-rash.jpg", so matching is done on a
// reduced form of both: lowercase, no punctuation, no bracketed region or
// version tags, no leading article.
#pragma once

#include <string>
#include <vector>

namespace retro3do {

// The reduced form two names are compared by. Exposed because it is the whole
// behaviour of this file and the only thing worth testing directly.
std::string cover_key(const std::string& name);

// File extensions accepted as artwork, lowercase and with the dot.
const std::vector<std::string>& cover_extensions();

// Pick the best match for `game_name` out of `files`, which is a plain list of
// file names in the covers folder. Returns an empty string when nothing
// matches well enough; a wrong cover on a game is worse than none.
std::string match_cover(const std::string& game_name,
                        const std::vector<std::string>& files);

}  // namespace retro3do
