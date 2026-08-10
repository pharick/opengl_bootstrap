cmake_minimum_required(VERSION 3.28)
project(opengl_bootstrap LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)          # no GNU extensions
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # in-file, so clangd works without extra flags
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# The compilation database lands in the build tree, but clangd looks next to the
# sources. Symlink it to the project root on every configure, so it always
# tracks whichever preset was configured most recently. Gitignored.
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    file(CREATE_LINK
         "${CMAKE_BINARY_DIR}/compile_commands.json"
         "${CMAKE_SOURCE_DIR}/compile_commands.json"
         SYMBOLIC
         RESULT _compile_commands_link)
    if(_compile_commands_link EQUAL 0)
        message(STATUS "compile_commands.json -> ${CMAKE_BINARY_DIR}/compile_commands.json")
    else()
        message(WARNING "could not link compile_commands.json: ${_compile_commands_link}")
    endif()
endif()

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(GLCORE_WERROR "Treat compiler warnings as errors" OFF)
option(GLCORE_BUILD_TESTS "Build the glcore unit tests" ON)
option(GLCORE_TIDY "Run clang-tidy during the build; any finding is an error" OFF)
set(GLCORE_SANITIZE "" CACHE STRING "Sanitizers to enable, e.g. 'address,undefined'")
set(GLCORE_GL_VERSION "3.3" CACHE STRING "Requested OpenGL core version (3.3 or 4.1)")

# gltut targets GLSL 3.30, so 3.3 is the default. Bump to 4.1 (macOS ceiling) if a
# later chapter needs it -- nothing else in the project has to change.
string(REPLACE "." ";" _gl_version_parts "${GLCORE_GL_VERSION}")
list(GET _gl_version_parts 0 GLCORE_GL_MAJOR)
list(GET _gl_version_parts 1 GLCORE_GL_MINOR)
message(STATUS "OpenGL target: ${GLCORE_GL_MAJOR}.${GLCORE_GL_MINOR} core profile")

# ---------------------------------------------------------------------------
# Dependencies (Homebrew / system)
# ---------------------------------------------------------------------------
# CMake does not search the Homebrew prefix by default on Apple Silicon.
if(APPLE)
    execute_process(COMMAND brew --prefix
                    OUTPUT_VARIABLE HOMEBREW_PREFIX
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET)
    if(HOMEBREW_PREFIX)
        list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_PREFIX}")
    endif()
endif()

find_package(OpenGL REQUIRED)
find_package(glfw3 3.4 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm CONFIG REQUIRED)

# Only needed from Tutorial 7 (gltut XML meshes). Absence is not an error.
find_package(tinyxml2 CONFIG QUIET)
if(tinyxml2_FOUND)
    set(GLCORE_WITH_MESH_XML ON)
    message(STATUS "tinyxml2 found -- gltut XML mesh loading enabled")
else()
    set(GLCORE_WITH_MESH_XML OFF)
    message(STATUS "tinyxml2 NOT found -- gltut XML mesh loading disabled "
                   "(needed from Tutorial 7; install with: brew install tinyxml2)")
endif()

# ---------------------------------------------------------------------------
# Sanitizers (applied to everything, including third_party, so ASan interposes)
# ---------------------------------------------------------------------------
if(GLCORE_SANITIZE)
    message(STATUS "Sanitizers: ${GLCORE_SANITIZE}")
    add_compile_options(-fsanitize=${GLCORE_SANITIZE} -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${GLCORE_SANITIZE})
endif()

include(CompilerWarnings)
include(AddTutorial)
include(Lint)

add_subdirectory(third_party)
add_subdirectory(glcore)
add_subdirectory(tutorials)

if(GLCORE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
