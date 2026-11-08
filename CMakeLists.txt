# SFML 3 Framework, and the Mud Cats demo, with CMake.
#
# The Makefile is the everyday build on Linux. This is the one for
# everything else: Visual Studio on Windows, Xcode or the command line on
# macOS, and any Linux. It downloads and builds SFML 3 itself, linked in
# statically, so the result runs without SFML installed:
#
#   cmake -B build-cmake -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-cmake --config Release
#
# The game lands in build-cmake/mudcats/ with its assets and shaders next
# to it: MudCats.exe on Windows, Mud Cats.app on macOS, MudCats on Linux.
# To make something to hand to someone (a zip on Windows and Linux, a disk
# image on macOS):
#
#   cmake --build build-cmake --config Release --target package
#
# For a Mac build that runs on both Apple Silicon and Intel, add
# -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" to the first line. The Linux
# AppImage is made from the package by tools/package/appimage.sh. The
# GitHub workflow in .github/workflows/build.yml does all of it, on all
# three systems.
#
# Linux needs the usual development packages for SFML first (on Debian or
# Ubuntu: libx11-dev libxrandr-dev libxcursor-dev libxi-dev libudev-dev
# libgl1-mesa-dev). To build against an SFML 3 that's
# already installed instead, add -DMUDCATS_SYSTEM_SFML=ON.

cmake_minimum_required(VERSION 3.24)
project(MudCats VERSION 0.14.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(MUDCATS_SYSTEM_SFML "Use an installed SFML 3 instead of downloading it" OFF)

# ---------------------------------------------------------------- SFML

if(MUDCATS_SYSTEM_SFML)
    find_package(SFML 3 REQUIRED COMPONENTS Graphics Audio Window System)
else()
    include(FetchContent)
    if(MSVC)
        # The C++ runtime linked in, everywhere, so Windows needs no
        # redistributable installed (SFML's and ours have to match).
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
    # SFML builds freetype, Vorbis and FLAC itself and links them in, instead
    # of using the system's: those differ between Linux distributions, and
    # the game has to run on ones it wasn't built on.
    set(SFML_USE_SYSTEM_DEPS OFF CACHE BOOL "" FORCE)
    if(MSVC)
        set(SFML_USE_STATIC_STD_LIBS ON CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 3.0.2
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(SFML)
endif()

# ---------------------------------------------------------------- the framework

file(GLOB FRAMEWORK_SOURCES CONFIGURE_DEPENDS
    src/core/*.cpp src/framework/*.cpp src/gui/*.cpp src/scenes/*.cpp src/utilities/*.cpp)
add_library(sfml_framework STATIC ${FRAMEWORK_SOURCES})
target_include_directories(sfml_framework PUBLIC include)
target_link_libraries(sfml_framework PUBLIC SFML::Graphics SFML::Audio SFML::Window SFML::System)
if(MSVC)
    target_compile_options(sfml_framework PUBLIC /W3 /utf-8 /permissive- /Zc:__cplusplus)
    target_compile_definitions(sfml_framework PUBLIC _USE_MATH_DEFINES NOMINMAX)
else()
    target_compile_options(sfml_framework PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------- Mud Cats

file(GLOB MUDCATS_SOURCES CONFIGURE_DEPENDS examples/mudcats/*.cpp)
list(FILTER MUDCATS_SOURCES EXCLUDE REGEX "/tests\\.cpp$")

# A window, not a console, on Windows; an app bundle on macOS.
if(WIN32)
    # The icon Explorer shows for the .exe.
    file(WRITE "${CMAKE_BINARY_DIR}/mudcats.rc" "IDI_ICON1 ICON \"${CMAKE_SOURCE_DIR}/assets/mudcats/mudcats.ico\"\n")
    list(APPEND MUDCATS_SOURCES "${CMAKE_BINARY_DIR}/mudcats.rc")
endif()
add_executable(mudcats WIN32 MACOSX_BUNDLE ${MUDCATS_SOURCES})
target_link_libraries(mudcats PRIVATE sfml_framework)
if(WIN32)
    target_link_libraries(mudcats PRIVATE SFML::Main)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # The C++ runtime in the program, so it doesn't depend on the version
    # the player's distribution happens to have.
    target_link_options(mudcats PRIVATE -static-libstdc++ -static-libgcc)
endif()
if(APPLE)
    set(MUDCATS_NAME "Mud Cats")
else()
    set(MUDCATS_NAME "MudCats")
endif()
set_target_properties(mudcats PROPERTIES
    OUTPUT_NAME "${MUDCATS_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/mudcats"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/mudcats"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/mudcats"
    MACOSX_BUNDLE_BUNDLE_NAME "Mud Cats"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.jamesbouchat.mudcats"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_ICON_FILE "mudcats.icns")

# What the game reads at run time: its sounds and pictures, the font, and
# the shaders. Copied next to the program after every build (into the
# bundle's Resources on macOS), and into the zip.
set(MUDCATS_DATA_DIRS assets/mudcats shaders/mudcats)
set(MUDCATS_DATA_FILES assets/fonts/pixel-font-bitmap.ttf shaders/bloom_extract.frag shaders/bloom_blur_v.frag)
if(APPLE)
    set(MUDCATS_DATA_DEST "$<TARGET_BUNDLE_CONTENT_DIR:mudcats>/Resources")
else()
    set(MUDCATS_DATA_DEST "$<TARGET_FILE_DIR:mudcats>")
endif()
foreach(dir ${MUDCATS_DATA_DIRS})
    add_custom_command(TARGET mudcats POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/${dir}" "${MUDCATS_DATA_DEST}/${dir}")
endforeach()
foreach(file ${MUDCATS_DATA_FILES})
    get_filename_component(file_dir "${file}" DIRECTORY)
    add_custom_command(TARGET mudcats POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MUDCATS_DATA_DEST}/${file_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/${file}" "${MUDCATS_DATA_DEST}/${file}")
endforeach()
if(APPLE AND EXISTS "${CMAKE_SOURCE_DIR}/assets/mudcats/mudcats.icns")
    add_custom_command(TARGET mudcats POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/assets/mudcats/mudcats.icns" "${MUDCATS_DATA_DEST}/mudcats.icns")
endif()

# ---------------------------------------------------------------- the package

if(APPLE)
    # The bundle already holds everything it needs, in Resources.
    install(TARGETS mudcats BUNDLE DESTINATION . COMPONENT game)
    # Signed, so macOS will open it: "-" signs it ad hoc, as nobody in
    # particular, which is enough for a Mac to run it once the player has
    # allowed it. Give a Developer ID here to sign it properly.
    set(MUDCATS_SIGN_IDENTITY "-" CACHE STRING "codesign identity for the app; - for ad hoc")
    install(CODE "execute_process(COMMAND codesign --force --deep --sign \"${MUDCATS_SIGN_IDENTITY}\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/Mud Cats.app\" COMMAND_ERROR_IS_FATAL ANY)"
            COMPONENT game)
else()
    install(TARGETS mudcats RUNTIME DESTINATION . COMPONENT game)
    foreach(dir ${MUDCATS_DATA_DIRS})
        get_filename_component(parent "${dir}" DIRECTORY)
        install(DIRECTORY "${dir}" DESTINATION "./${parent}" COMPONENT game)
    endforeach()
    foreach(file ${MUDCATS_DATA_FILES})
        get_filename_component(file_dir "${file}" DIRECTORY)
        install(FILES "${file}" DESTINATION "./${file_dir}" COMPONENT game)
    endforeach()
endif()
install(FILES examples/mudcats/README.md LICENSE DESTINATION . COMPONENT game)
# nlohmann/json is compiled in, so its licence goes with it.
install(FILES third_party/nlohmann/LICENSE.MIT DESTINATION . RENAME LICENSE-nlohmann-json.txt COMPONENT game)

# Only the game: SFML's own install rules (its headers and libraries) stay out of it.
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${PROJECT_NAME};game;/")
set(CPACK_PACKAGE_NAME "MudCats")
set(CPACK_PACKAGE_VENDOR "James Bouchat")
if(APPLE)
    # A disk image: the app, with a link to Applications to drag it onto.
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_DMG_VOLUME_NAME "Mud Cats")
    set(MUDCATS_SYSTEM_LABEL macOS)
else()
    set(CPACK_GENERATOR ZIP)
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    set(MUDCATS_SYSTEM_LABEL "${CMAKE_SYSTEM_NAME}")
endif()
set(CPACK_PACKAGE_FILE_NAME "MudCats-${PROJECT_VERSION}-${MUDCATS_SYSTEM_LABEL}")
include(CPack)
