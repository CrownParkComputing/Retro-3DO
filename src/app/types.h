// Fixed-width types for the application.
//
// These used to arrive with the core's own headers, because the application and
// the emulator were one tree. They are not any more: the core is reached through
// retro3do.h and nothing else, so the front end declares the handful of short
// names its own code is written in rather than reaching across the boundary for
// them. They are the same aliases over the same standard types — nothing here is
// shared with the core, and nothing depends on it.
#pragma once

#include <cstddef>
#include <cstdint>

namespace retro3do {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s8  = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

}  // namespace retro3do
