# An INTERFACE target carrying the project's warning policy.
#
# Link it into glcore and every tutorial -- never into third_party, whose code we
# do not control (its headers are additionally marked SYSTEM).
#
# -Werror is opt-in via GLCORE_WERROR (the `dev` preset turns it on). It is
# deliberately off by default: a deliberately-unused variable while working
# through a chapter should not stop the build.

add_library(glcore_warnings INTERFACE)

set(_glcore_clang_gnu_warnings
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wnon-virtual-dtor
    -Wold-style-cast          # humangl had a `(void*)0` in its attribute setup
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wimplicit-fallthrough)

set(_glcore_msvc_warnings /W4 /permissive- /w14640)

target_compile_options(glcore_warnings INTERFACE
    $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:${_glcore_clang_gnu_warnings}>
    $<$<CXX_COMPILER_ID:MSVC>:${_glcore_msvc_warnings}>
    $<$<AND:$<BOOL:${GLCORE_WERROR}>,$<CXX_COMPILER_ID:Clang,AppleClang,GNU>>:-Werror>
    $<$<AND:$<BOOL:${GLCORE_WERROR}>,$<CXX_COMPILER_ID:MSVC>>:/WX>)
