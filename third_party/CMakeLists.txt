# Third-party code. Nothing here gets glcore_warnings, and every include
# directory is marked SYSTEM so warnings from these headers never reach us.

# ---------------------------------------------------------------------------
# Dear ImGui -- git submodule, pinned to tag v1.92.9b-docking
# ---------------------------------------------------------------------------
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
if(NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
    message(FATAL_ERROR
        "Dear ImGui sources not found in third_party/imgui.\n"
        "Run: git submodule update --init --recursive")
endif()

add_library(imgui STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp")

target_include_directories(imgui SYSTEM PUBLIC
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends")

# imgui_impl_opengl3.cpp carries its own self-contained GL loader, so it must
# not be told to use GLEW -- the two would collide in that translation unit.
target_compile_definitions(imgui PUBLIC
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    $<$<PLATFORM_ID:Darwin>:GL_SILENCE_DEPRECATION>)

target_link_libraries(imgui PUBLIC glfw)
add_library(third_party::imgui ALIAS imgui)

# ---------------------------------------------------------------------------
# gli -- git submodule, header-only. Reads KTX and DDS.
# ---------------------------------------------------------------------------
# gli vendors its own glm under external/, which is deliberately NOT put on the
# include path: glm types cross the gli/glcore boundary, so two different glm
# versions in one build would be an ODR violation. gli compiles fine against the
# project's Homebrew glm, so there is exactly one.
#
# SYSTEM is load-bearing here, unlike for most vendored headers -- gli emits
# ~390 warnings under this project's warning set.
set(GLI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gli")
if(NOT EXISTS "${GLI_DIR}/gli/gli.hpp")
    message(FATAL_ERROR
        "gli sources not found in third_party/gli.\n"
        "Run: git submodule update --init --recursive")
endif()

add_library(gli INTERFACE)
target_include_directories(gli SYSTEM INTERFACE "${GLI_DIR}")
target_link_libraries(gli INTERFACE glm::glm)
add_library(third_party::gli ALIAS gli)

# The test framework is Catch2, from Homebrew -- see tests/CMakeLists.txt.
# Nothing vendored here for it.
