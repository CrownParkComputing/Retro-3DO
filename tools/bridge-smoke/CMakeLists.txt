# Builds the iOS bridge and its smoke test on an ordinary desktop.
#
# Separate from the main build on purpose. The point of this harness is that
# the half of the iOS app which can be tested without a Mac gets tested without
# a Mac, so it must not need SDL, Dear ImGui or either of the submodules the
# app build pulls in — only the core and the bridge.
#
#   cmake -S tools/bridge-smoke -B build-bridge -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-bridge
#   ./build-bridge/bridge_smoke /tmp/3do-support
#
# Optionally give it a disc image and the kind it should be judged to be
# (0 refused, 1 system ROM, 2 disc):
#
#   ./build-bridge/bridge_smoke /tmp/3do-support "some-saturn-game.chd" 0
cmake_minimum_required(VERSION 3.20)
project(retro3do_bridge_smoke CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(R3_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# libchdr's own CMake wants a tests directory this checkout does not carry, so
# its sources and its vendored dependencies are compiled directly.
file(GLOB_RECURSE CHDR_SOURCES
    ${R3_ROOT}/third_party/libchdr/src/*.c
    ${R3_ROOT}/third_party/libchdr/deps/*/*.c)
add_library(chdr-static STATIC ${CHDR_SOURCES})
file(GLOB CHDR_DEP_DIRS ${R3_ROOT}/third_party/libchdr/deps/*)
target_include_directories(chdr-static PUBLIC
    ${R3_ROOT}/third_party/libchdr/include ${CHDR_DEP_DIRS})

file(GLOB CORE_SOURCES ${R3_ROOT}/src/core/*.cpp)
add_library(retro3do_core_host STATIC
    ${CORE_SOURCES}
    ${R3_ROOT}/src/ui/game_library.cpp)
target_include_directories(retro3do_core_host PUBLIC
    ${R3_ROOT}/src ${R3_ROOT}/src/core ${R3_ROOT}/src/ui)
target_link_libraries(retro3do_core_host PRIVATE chdr-static)

find_package(Threads REQUIRED)
add_executable(bridge_smoke
    ${R3_ROOT}/ios/bridge/retro3do_bridge.cpp
    ${R3_ROOT}/tools/bridge_smoke.c)
target_include_directories(bridge_smoke PRIVATE ${R3_ROOT}/ios/bridge)
target_link_libraries(bridge_smoke PRIVATE
    retro3do_core_host chdr-static Threads::Threads)
