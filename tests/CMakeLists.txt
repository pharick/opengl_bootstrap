# Tests cover the parts of glcore that need no GL context: source expansion,
# path resolution and (later) the matrix stack.

add_executable(glcore_tests
    test_main.cpp
    test_glsl_source.cpp
    test_matrix_stack.cpp
    test_paths.cpp)

target_link_libraries(glcore_tests PRIVATE glcore::glcore third_party::doctest glcore_warnings)

add_test(NAME glcore_tests COMMAND glcore_tests)
