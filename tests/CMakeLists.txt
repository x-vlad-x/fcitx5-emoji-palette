add_executable(core-tests core_tests.cpp)
target_link_libraries(core-tests PRIVATE EmojiPalette::Core)
target_compile_features(core-tests PRIVATE cxx_std_20)
target_compile_options(
    core-tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        $<$<BOOL:${EMOJI_PALETTE_WARNINGS_AS_ERRORS}>:-Werror>
)
add_test(NAME core-tests COMMAND core-tests)

add_executable(ipc-tests ipc_tests.cpp)
target_link_libraries(ipc-tests PRIVATE EmojiPalette::Ipc)
target_compile_features(ipc-tests PRIVATE cxx_std_20)
target_compile_options(
    ipc-tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        $<$<BOOL:${EMOJI_PALETTE_WARNINGS_AS_ERRORS}>:-Werror>
)
add_test(NAME ipc-tests COMMAND ipc-tests)

add_executable(selection-controller-tests selection_controller_tests.cpp)
target_link_libraries(selection-controller-tests PRIVATE EmojiPalette::Runtime)
target_compile_features(selection-controller-tests PRIVATE cxx_std_20)
target_compile_options(
    selection-controller-tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        $<$<BOOL:${EMOJI_PALETTE_WARNINGS_AS_ERRORS}>:-Werror>
)
add_test(NAME selection-controller-tests COMMAND selection-controller-tests)

if(TARGET EmojiPalette::Shortcut)
    add_executable(shortcut-matcher-tests shortcut_matcher_tests.cpp)
    target_link_libraries(shortcut-matcher-tests PRIVATE EmojiPalette::Shortcut)
    target_compile_features(shortcut-matcher-tests PRIVATE cxx_std_20)
    target_compile_options(
        shortcut-matcher-tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            $<$<BOOL:${EMOJI_PALETTE_WARNINGS_AS_ERRORS}>:-Werror>
    )
    add_test(NAME shortcut-matcher-tests COMMAND shortcut-matcher-tests)
endif()

add_executable(core-benchmark core_benchmark.cpp)
target_link_libraries(core-benchmark PRIVATE EmojiPalette::Core)
target_compile_features(core-benchmark PRIVATE cxx_std_20)
add_test(NAME core-benchmark COMMAND core-benchmark)

add_test(
    NAME unicode-generator-tests
    COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/unicode_generator_tests.py
        ${PROJECT_SOURCE_DIR}
)

if(TARGET EmojiPalette::Ui)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)
    add_executable(helper-tests helper_tests.cpp)
    set_target_properties(helper-tests PROPERTIES AUTOMOC ON)
    target_link_libraries(
        helper-tests
        PRIVATE
            EmojiPalette::Ui
            Qt6::Test
    )
    target_compile_features(helper-tests PRIVATE cxx_std_20)
    target_compile_options(
        helper-tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            $<$<BOOL:${EMOJI_PALETTE_WARNINGS_AS_ERRORS}>:-Werror>
    )
    add_test(NAME helper-tests COMMAND helper-tests)
    set_tests_properties(
        helper-tests
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
