find_package(Fcitx5Core 5.1 REQUIRED)
find_package(Fcitx5ModuleDBus 5.1 REQUIRED)

add_fcitx5_addon(emoji-palette-addon emoji_palette_addon.cpp)
target_compile_features(emoji-palette-addon PRIVATE cxx_std_20)
target_link_libraries(
    emoji-palette-addon
    PRIVATE
        EmojiPalette::Runtime
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

configure_file(emoji-palette.conf.in emoji-palette.conf @ONLY)

install(TARGETS emoji-palette-addon DESTINATION "${FCITX_INSTALL_ADDONDIR}")
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/emoji-palette.conf"
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon"
    COMPONENT config
)
