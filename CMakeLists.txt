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
option(EMOJI_PALETTE_BUILD_FCITX_ADDON "Build the Fcitx5 addon" ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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
        $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU>>:-O2>
        $<$<BOOL:${EMOJI_PALETTE_WARNINGS_AS_ERRORS}>:-Werror>
)

add_library(
    emoji-palette-ipc
    src/ipc/protocol.cpp
    src/ipc/session.cpp
)
add_library(EmojiPalette::Ipc ALIAS emoji-palette-ipc)
target_compile_features(emoji-palette-ipc PUBLIC cxx_std_20)
target_include_directories(
    emoji-palette-ipc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(emoji-palette-ipc PUBLIC EmojiPalette::Core)
target_compile_options(
    emoji-palette-ipc
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        $<$<BOOL:${EMOJI_PALETTE_WARNINGS_AS_ERRORS}>:-Werror>
)

add_library(
    emoji-palette-runtime
    src/runtime/selection_controller.cpp
)
add_library(EmojiPalette::Runtime ALIAS emoji-palette-runtime)
target_compile_features(emoji-palette-runtime PUBLIC cxx_std_20)
target_include_directories(
    emoji-palette-runtime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(
    emoji-palette-runtime
    PUBLIC
        EmojiPalette::Core
        EmojiPalette::Ipc
)
target_compile_options(
    emoji-palette-runtime
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        $<$<BOOL:${EMOJI_PALETTE_WARNINGS_AS_ERRORS}>:-Werror>
)

if(EMOJI_PALETTE_BUILD_FCITX_ADDON)
    add_subdirectory(src/addon)
endif()

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_subdirectory(tests)
endif()
