find_package(Qt6 6.6 REQUIRED COMPONENTS Core DBus Gui Widgets LinguistTools)
find_package(LayerShellQt 6.6 REQUIRED)

set(CMAKE_AUTOMOC ON)

add_library(
    emoji-palette-ui
    STATIC
    emoji_list_model.cpp
    palette_session.cpp
    palette_window.cpp
)
add_library(EmojiPalette::Ui ALIAS emoji-palette-ui)
target_compile_features(emoji-palette-ui PUBLIC cxx_std_20)
target_include_directories(emoji-palette-ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
    emoji-palette-ui
    PUBLIC
        EmojiPalette::Core
        EmojiPalette::Ipc
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Widgets
        LayerShellQt::Interface
)
target_compile_options(
    emoji-palette-ui
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        $<$<BOOL:${EMOJI_PALETTE_WARNINGS_AS_ERRORS}>:-Werror>
)

add_executable(fcitx5-emoji-palette-ui main.cpp)
target_link_libraries(fcitx5-emoji-palette-ui PRIVATE EmojiPalette::Ui)
target_compile_features(fcitx5-emoji-palette-ui PRIVATE cxx_std_20)
target_compile_definitions(
    fcitx5-emoji-palette-ui
    PRIVATE
        EMOJI_PALETTE_VERSION="${PROJECT_VERSION}"
)
target_compile_options(
    fcitx5-emoji-palette-ui
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        $<$<BOOL:${EMOJI_PALETTE_WARNINGS_AS_ERRORS}>:-Werror>
)

set(
    translation_sources
    ${PROJECT_SOURCE_DIR}/translations/emoji-palette_de.ts
    ${PROJECT_SOURCE_DIR}/translations/emoji-palette_ru.ts
)
qt_add_lrelease(
    TS_FILES ${translation_sources}
    QM_FILES_OUTPUT_VARIABLE translation_files
)
qt_add_resources(
    fcitx5-emoji-palette-ui
    ui-translations
    PREFIX "/i18n"
    BASE "${CMAKE_CURRENT_BINARY_DIR}"
    FILES ${translation_files}
)

configure_file(
    org.fcitx.Fcitx5.EmojiPalette1.service.in
    org.fcitx.Fcitx5.EmojiPalette1.service
    @ONLY
)

install(TARGETS fcitx5-emoji-palette-ui DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/org.fcitx.Fcitx5.EmojiPalette1.service"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/services"
)
install(
    FILES ${translation_files}
    DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5-emoji-palette/translations"
)
