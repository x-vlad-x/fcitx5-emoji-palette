find_package(Fcitx5Core 5.1 REQUIRED)
find_package(Fcitx5ModuleDBus 5.1 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XkbCommon REQUIRED IMPORTED_TARGET "xkbcommon>=1.0")

add_library(emoji-palette-shortcut STATIC key_router.cpp shortcut_matcher.cpp)
target_compile_features(emoji-palette-shortcut PUBLIC cxx_std_20)
target_include_directories(
    emoji-palette-shortcut
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(
    emoji-palette-shortcut
    PUBLIC
        EmojiPalette::Ipc
        Fcitx5::Core
    PRIVATE
        PkgConfig::XkbCommon
)
target_compile_options(
    emoji-palette-shortcut
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        $<$<BOOL:${EMOJI_PALETTE_WARNINGS_AS_ERRORS}>:-Werror>
)
add_library(EmojiPalette::Shortcut ALIAS emoji-palette-shortcut)

add_fcitx5_addon(emoji-palette-addon emoji_palette_addon.cpp)
target_compile_features(emoji-palette-addon PRIVATE cxx_std_20)
target_link_libraries(
    emoji-palette-addon
    PRIVATE
        EmojiPalette::Runtime
        EmojiPalette::Shortcut
        Fcitx5::Core
        Fcitx5::Module::DBus
)
target_compile_options(
    emoji-palette-addon
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        $<$<BOOL:${EMOJI_PALETTE_WARNINGS_AS_ERRORS}>:-Werror>
)
set_target_properties(emoji-palette-addon PROPERTIES OUTPUT_NAME emoji-palette)

configure_file(emojipalette.conf.in emojipalette.conf @ONLY)

install(TARGETS emoji-palette-addon DESTINATION "${FCITX_INSTALL_ADDONDIR}")
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/emojipalette.conf"
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon"
    COMPONENT config
)
