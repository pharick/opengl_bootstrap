# `format`, `format-check` and `tidy` targets.
#
# Homebrew keeps clang-format/clang-tidy out of the default PATH, so both are
# looked up in the LLVM keg as well.

find_program(CLANG_FORMAT_EXE
             NAMES clang-format
             HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)

find_program(CLANG_TIDY_EXE
             NAMES clang-tidy
             HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)

# The sources we own. third_party is never formatted or linted.
file(GLOB_RECURSE GLCORE_LINT_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_SOURCE_DIR}/glcore/*.cpp"
     "${CMAKE_SOURCE_DIR}/glcore/*.hpp"
     "${CMAKE_SOURCE_DIR}/tests/*.cpp"
     "${CMAKE_SOURCE_DIR}/tutorials/*.cpp")

# clang-format treats unknown extensions as C++, which is close enough for GLSL
# and keeps the shaders on the same tabs-indent / spaces-align rules as the C++.
# These are formatted but never passed to clang-tidy.
file(GLOB_RECURSE GLCORE_SHADER_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_SOURCE_DIR}/assets/*.glsl"
     "${CMAKE_SOURCE_DIR}/tutorials/*.glsl"
     "${CMAKE_SOURCE_DIR}/tutorials/*.vert"
     "${CMAKE_SOURCE_DIR}/tutorials/*.frag")

if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${GLCORE_LINT_SOURCES} ${GLCORE_SHADER_SOURCES}
        COMMENT "Formatting C++ and GLSL with ${CLANG_FORMAT_EXE}"
        VERBATIM)

    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run -Werror
                ${GLCORE_LINT_SOURCES} ${GLCORE_SHADER_SOURCES}
        COMMENT "Checking C++ and GLSL formatting"
        VERBATIM)
else()
    message(STATUS "clang-format not found -- 'format' target unavailable")
endif()

if(CLANG_TIDY_EXE)
    # Homebrew's clang-tidy does not know where Apple's SDK keeps libc++, and
    # compile_commands.json does not carry an -isysroot because Apple's own
    # clang has it built in. Without this it fails to find <format>, <expected>
    # and friends, and then reports garbage from a half-parsed AST -- including
    # dozens of bogus "can be made static" findings.
    # CMAKE_OSX_SYSROOT is often empty (CMake leaves it unset when the default
    # SDK is fine for the *compiler*), so ask xcrun directly.
    set(_tidy_sysroot_args "")
    if(APPLE)
        set(_tidy_sdk "${CMAKE_OSX_SYSROOT}")
        if(NOT _tidy_sdk)
            execute_process(COMMAND xcrun --show-sdk-path
                            OUTPUT_VARIABLE _tidy_sdk
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                            ERROR_QUIET)
        endif()
        if(_tidy_sdk)
            set(_tidy_sysroot_args --extra-arg=-isysroot --extra-arg=${_tidy_sdk})
        else()
            message(WARNING
                "could not determine the macOS SDK path; clang-tidy will not find libc++ "
                "and its results will be unreliable")
        endif()
    endif()

    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXE}
                -p ${CMAKE_BINARY_DIR}
                --quiet
                --warnings-as-errors=*
                ${_tidy_sysroot_args}
                ${GLCORE_LINT_SOURCES}
        COMMENT "Running clang-tidy"
        VERBATIM)
    # With GLCORE_TIDY on (the dev preset), clang-tidy also runs as part of
    # compilation and any finding fails the build. Invoked this way it is handed
    # the compile command directly, so no -p is needed -- but it still needs the
    # sysroot for the same reason as above.
    if(GLCORE_TIDY)
        set(GLCORE_TIDY_COMMAND "${CLANG_TIDY_EXE}" --quiet --warnings-as-errors=*
                                ${_tidy_sysroot_args})
        message(STATUS "clang-tidy runs during the build and findings are errors")
    endif()
else()
    message(STATUS "clang-tidy not found -- 'tidy' target unavailable "
                   "(install with: brew install llvm)")
endif()
