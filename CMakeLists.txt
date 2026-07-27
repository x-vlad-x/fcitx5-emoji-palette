cmake_minimum_required(VERSION 3.27)

project(
    fcitx5-emoji-palette
    VERSION 0.1.0
    DESCRIPTION "Native emoji picker for Fcitx 5"
    LANGUAGES CXX
)

include(GNUInstallDirs)
include(CTest)

option(EMOJI_PALETTE_WARNINGS_AS_ERRORS "Treat project warnings as errors" ON)

add_library(
    emoji-palette-core
    src/core/geometry.cpp
    src/core/keyboard.cpp
    src/core/state.cpp
    src/core/utf8.cpp
    src/model/catalog.cpp
)
add_library(EmojiPalette::Core ALIAS emoji-palette-core)

target_compile_features(emoji-palette-core PUBLIC cxx_std_20)
target_include_directories(
    emoji-palette-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(
    emoji-palette-core
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        $<$<BOOL:${EMOJI_PALETTE_WARNINGS_AS_ERRORS}>:-Werror>
)

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_subdirectory(tests)
endif()
