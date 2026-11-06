# CMake build used for the iOS port (desktop builds still use the Makefile).
#
# Expects SDL2 and SDL2_mixer source trees in deps/SDL and deps/SDL_mixer
# (the GitHub Actions workflow in .github/workflows/ios.yml fetches them)
# and fokker.dks built in the repository root.

cmake_minimum_required(VERSION 3.20)
project(TriplaneClassic LANGUAGES C CXX OBJC)

set(CMAKE_CXX_STANDARD 11)
set(TRIPLANE_VERSION "1.0.11")

# --- SDL2 (static) -----------------------------------------------------------
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
add_subdirectory(deps/SDL EXCLUDE_FROM_ALL)

# --- SDL2_mixer (static, WAV + MOD only) -------------------------------------
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_VENDORED ON CACHE BOOL "" FORCE)
set(SDL2MIXER_DEPS_SHARED OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD ON CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD_XMP ON CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD_MODPLUG OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_FLAC OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MP3 OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MIDI OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_OPUS OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_WAVPACK OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_GME OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_WAVE ON CACHE BOOL "" FORCE)
add_subdirectory(deps/SDL_mixer EXCLUDE_FROM_ALL)

# --- The game ----------------------------------------------------------------
set(TRIPLANE_SOURCES
    src/triplane.cpp
    src/settings.cpp
    src/gfx/bitmap.cpp src/gfx/font.cpp src/gfx/gfx.cpp
    src/gfx/fades.cpp src/gfx/extra.cpp
    src/util/wutil.cpp src/util/random.cpp
    src/io/sdl_compat.cpp src/io/video.cpp src/io/mouse.cpp
    src/io/dksfile.cpp src/io/timing.cpp src/io/joystick.cpp
    src/io/sound.cpp src/io/touch.cpp
    src/world/tripai.cpp src/world/tripmis.cpp src/world/terrain.cpp
    src/world/fobjects.cpp src/world/tmexept.cpp src/world/plane.cpp
    src/world/tripaudio.cpp
    src/menus/tripmenu.cpp
)

set(TRIPLANE_RESOURCES
    ${CMAKE_SOURCE_DIR}/fokker.dks
    ${CMAKE_SOURCE_DIR}/ios/LaunchScreen.storyboard
)

add_executable(triplane MACOSX_BUNDLE ${TRIPLANE_SOURCES} ${TRIPLANE_RESOURCES})

set_source_files_properties(${TRIPLANE_RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

target_include_directories(triplane PRIVATE src)

target_compile_definitions(triplane PRIVATE
    HAVE_SDL_MIXER
    TRIPLANE_IOS
    TRIPLANE_TOUCH
    TRIPLANE_DATA="."
    TRIPLANE_VERSION="${TRIPLANE_VERSION}"
)

target_link_libraries(triplane PRIVATE
    SDL2::SDL2main
    SDL2::SDL2-static
    SDL2_mixer::SDL2_mixer-static
    "-framework AVFoundation"
    "-framework AudioToolbox"
    "-framework CoreAudio"
    "-framework CoreGraphics"
    "-framework CoreHaptics"
    "-framework CoreMotion"
    "-framework Foundation"
    "-framework GameController"
    "-framework Metal"
    "-framework OpenGLES"
    "-framework QuartzCore"
    "-framework UIKit"
)

set_target_properties(triplane PROPERTIES
    OUTPUT_NAME "Triplane"
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/ios/Info.plist
    MACOSX_BUNDLE_GUI_IDENTIFIER "fi.konemestari.triplane"
    MACOSX_BUNDLE_BUNDLE_NAME "Triplane"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${TRIPLANE_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "1"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "fi.konemestari.triplane"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
)
