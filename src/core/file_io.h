#pragma once

#include <cstdio>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace retro3do::file_io {
inline std::FILE* from_descriptor(int fd, const char* mode) {
#if defined(_WIN32)
    return ::_fdopen(fd, mode);
#else
    return ::fdopen(fd, mode);
#endif
}
inline void close_descriptor(int fd) {
#if defined(_WIN32)
    ::_close(fd);
#else
    ::close(fd);
#endif
}
}  // namespace retro3do::file_io
