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
