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
# to it: MudCats.exe on Windows, Mud Cats.app on macOS, MudCats on Linux. To make a zip to hand to someone:
#
#   cmake --build build-cmake --config Release --target package
#
# Linux needs the usual development packages for SFML first (on Debian or
# Ubuntu: libx11-dev libxrandr-dev libxcursor-dev libxi-dev libudev-dev
# libgl1-mesa-dev libfreetype-dev). To build against an SFML 3 that's
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
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
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

# ---------------------------------------------------------------- the zip

if(APPLE)
    install(TARGETS mudcats BUNDLE DESTINATION . COMPONENT game)
    set(MUDCATS_INSTALL_DATA "Mud Cats.app/Contents/Resources")
    install(FILES assets/mudcats/mudcats.icns DESTINATION "${MUDCATS_INSTALL_DATA}" COMPONENT game)
else()
    install(TARGETS mudcats RUNTIME DESTINATION . COMPONENT game)
    set(MUDCATS_INSTALL_DATA ".")
endif()
foreach(dir ${MUDCATS_DATA_DIRS})
    get_filename_component(parent "${dir}" DIRECTORY)
    install(DIRECTORY "${dir}" DESTINATION "${MUDCATS_INSTALL_DATA}/${parent}" COMPONENT game)
endforeach()
foreach(file ${MUDCATS_DATA_FILES})
    get_filename_component(file_dir "${file}" DIRECTORY)
    install(FILES "${file}" DESTINATION "${MUDCATS_INSTALL_DATA}/${file_dir}" COMPONENT game)
endforeach()
install(FILES examples/mudcats/README.md LICENSE DESTINATION . COMPONENT game)
# nlohmann/json is compiled in, so its licence goes with it.
install(FILES third_party/nlohmann/LICENSE.MIT DESTINATION . RENAME LICENSE-nlohmann-json.txt COMPONENT game)

# Only the game: SFML's own install rules (its headers and libraries) stay out of it.
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${PROJECT_NAME};game;/")
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME "MudCats")
set(CPACK_PACKAGE_FILE_NAME "MudCats-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
include(CPack)
