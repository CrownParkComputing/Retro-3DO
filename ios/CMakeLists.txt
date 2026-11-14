# The iOS application.
#
# Swift owns main(), the window and the whole of the interface. The emulator is
# retro3do_core, exactly as every other platform builds it, reached through one
# plain-C header. Nothing in this directory compiles SDL or Dear ImGui, and
# nothing in src/ui or src/platform is built for iOS at all.
#
# Why the front end is not shared: the App Store rejected this app under
# guideline 4.3 for sharing its screens with the developer's other apps. A
# front end written for the platform is the answer to that, and it is also the
# answer to the app being hard to use — the shared one asked an iPhone user to
# type file paths into a text field.

enable_language(Swift)
if(CMAKE_Swift_COMPILER_VERSION VERSION_LESS 5.5)
    message(FATAL_ERROR "Swift 5.5 or newer is required (found ${CMAKE_Swift_COMPILER_VERSION})")
endif()

set(RETRO3DO_SWIFT_SOURCES
    Retro3DO/Retro3DOApp.swift
    Retro3DO/Shelf.swift
    Retro3DO/Session.swift
    Retro3DO/Screen.swift
    Retro3DO/Sound.swift
    Retro3DO/Pad.swift
    Retro3DO/PadLayout.swift
    Retro3DO/Importer.swift
    Retro3DO/SettingsSheet.swift
    Retro3DO/GuideView.swift
    Retro3DO/Engine.swift
    Retro3DO/Theme.swift
)

# Listed file by file rather than globbed. A GLOB that silently misses a file
# produces a build that compiles and an app with a missing screen, and the
# first place anyone would look is the Swift.
foreach(f ${RETRO3DO_SWIFT_SOURCES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${f}")
        message(FATAL_ERROR "Swift source missing: ${f}")
    endif()
endforeach()

# retro3do_core is C++ and the bridge is C++; a Swift-only target would leave
# the C++ standard library unlinked. One C++ file in the target is what makes
# CMake link libc++.
set(RETRO3DO_BRIDGE_SOURCES
    bridge/retro3do_bridge.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/game_library.cpp
)

add_executable(Retro3DO MACOSX_BUNDLE
    ${RETRO3DO_SWIFT_SOURCES}
    ${RETRO3DO_BRIDGE_SOURCES}
)

# The icon. actool is part of Xcode and only runs under the Xcode generator, so
# the catalog is attached there and skipped elsewhere: a Ninja build still
# produces a running app, it just has a generic icon. That is the right way
# round — a missing icon must not stop anyone building and testing.
#
# All three lines are load-bearing. Attaching the catalog without
# MACOSX_PACKAGE_LOCATION compiles it but leaves CFBundleIcons out of the
# built Info.plist, and a bundle with no CFBundleIconName is accepted by the
# upload and then discarded in processing.
set(RETRO3DO_XCASSETS ${CMAKE_SOURCE_DIR}/assets/apple/Assets.xcassets)
if(CMAKE_GENERATOR STREQUAL "Xcode")
    target_sources(Retro3DO PRIVATE ${RETRO3DO_XCASSETS})
    set_source_files_properties(${RETRO3DO_XCASSETS} PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(Retro3DO PROPERTIES
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")
endif()

target_include_directories(Retro3DO PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/bridge
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/ui
)

target_link_libraries(Retro3DO PRIVATE retro3do_core)

# The frameworks the Swift uses. MetalKit draws the picture, CoreImage scales
# it, AVFoundation plays the sound and UniformTypeIdentifiers describes what
# the document picker will accept.
target_link_libraries(Retro3DO PRIVATE
    "-framework UIKit"
    "-framework SwiftUI"
    "-framework Metal"
    "-framework MetalKit"
    "-framework CoreImage"
    "-framework CoreGraphics"
    "-framework AVFoundation"
    "-framework UniformTypeIdentifiers"
)

set_target_properties(Retro3DO PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    # The CMake target is Retro3DO because that is the Xcode scheme name, but
    # the product keeps the name it has always shipped under. Renaming the
    # bundle would change CFBundleName on a live App Store record and break
    # every script that looks for Retro-3DO.app.
    OUTPUT_NAME "Retro-3DO"
    MACOSX_BUNDLE_EXECUTABLE_NAME "Retro-3DO"
    MACOSX_BUNDLE_BUNDLE_NAME "Retro-3DO"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${RETRO3DO_BUNDLE_ID}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${RETRO3DO_BUILD_NUMBER}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${RETRO3DO_MARKETING_VERSION}"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${RETRO3DO_BUNDLE_ID}"
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${RETRO3DO_APPLE_TEAM_ID}"
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "${RETRO3DO_IOS_MIN}"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"            # iPhone and iPad
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES"
    XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
    XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
    XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE "${RETRO3DO_CODE_SIGN_STYLE}"
    XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER "${RETRO3DO_PROVISIONING_PROFILE}"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${RETRO3DO_CODE_SIGN_IDENTITY}"
    # Swift reaches the engine through this one header, which is plain C on
    # purpose: the C++ underneath has std::string in its signatures and Swift
    # should never see it.
    XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER
        "${CMAKE_CURRENT_SOURCE_DIR}/bridge/retro3do_bridge.h"
    XCODE_ATTRIBUTE_SWIFT_VERSION "5.0"
    XCODE_ATTRIBUTE_ALWAYS_SEARCH_USER_PATHS "NO"
)
