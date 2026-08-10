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
# stb_image -- vendored single header (v2.30), no upstream package exists
# ---------------------------------------------------------------------------
add_library(stb STATIC "${CMAKE_CURRENT_SOURCE_DIR}/stb/stb_image_impl.cpp")
target_include_directories(stb SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/stb")
add_library(third_party::stb ALIAS stb)

# ---------------------------------------------------------------------------
# doctest -- vendored single header, header-only
# ---------------------------------------------------------------------------
add_library(doctest INTERFACE)
target_include_directories(doctest SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/doctest")
add_library(third_party::doctest ALIAS doctest)
