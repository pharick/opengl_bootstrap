# Tests cover the parts of glcore that need no GL context: source expansion,
# path resolution, the matrix stack, the cycle timer and the interpolators.
#
# Catch2 supplies main() via Catch2::Catch2WithMain, so there is no test_main.cpp.

find_package(Catch2 3 REQUIRED)
include(Catch)

add_executable(glcore_tests
    test_cycle_timer.cpp
    test_glsl_source.cpp
    test_interpolator.cpp
    test_matrix_stack.cpp
    test_paths.cpp)

target_link_libraries(glcore_tests
    PRIVATE glcore::glcore Catch2::Catch2WithMain glcore_warnings)

# Same inline clang-tidy the library and the tutorials get. Without this the
# test target was silently exempt, so a finding here only showed up when someone
# ran the `tidy` target -- the dev build would pass on code `tidy` rejected.
if(GLCORE_TIDY_COMMAND)
    set_target_properties(glcore_tests PROPERTIES CXX_CLANG_TIDY "${GLCORE_TIDY_COMMAND}")
endif()

# Registers every TEST_CASE as its own CTest entry, so `ctest -R "matrix"` and
# friends work, and a failure names the case rather than the whole binary.
catch_discover_tests(glcore_tests)
