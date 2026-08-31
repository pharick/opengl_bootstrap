add_library(glcore STATIC
    src/app.cpp
    src/buffer.cpp
    src/camera.cpp
    src/gl_check.cpp
    src/glsl_source.cpp
    src/imgui_layer.cpp
    src/input.cpp
    src/log.cpp
    src/matrix_stack.cpp
    src/paths.cpp
    src/program.cpp
    src/scoped_bind.cpp
    src/shader_watcher.cpp
    src/texture.cpp
    src/timer.cpp
    src/uniform_buffer.cpp
    src/vertex_array.cpp
    src/window.cpp)

# gltut XML mesh loading is optional: it is the only thing needing tinyxml2, and
# it is not required until Tutorial 7.
if(GLCORE_WITH_MESH_XML)
    target_sources(glcore PRIVATE src/mesh.cpp)
    target_link_libraries(glcore PUBLIC tinyxml2::tinyxml2)
    target_compile_definitions(glcore PUBLIC GLCORE_WITH_MESH_XML)
endif()

add_library(glcore::glcore ALIAS glcore)

if(GLCORE_TIDY_COMMAND)
    set_target_properties(glcore PROPERTIES CXX_CLANG_TIDY "${GLCORE_TIDY_COMMAND}")
endif()

target_include_directories(glcore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# All PUBLIC: glcore's headers expose GL types, GLFWwindow and glm types.
# Interface include dirs of IMPORTED targets are treated as SYSTEM, so the
# project's warning flags never fire inside these headers.
target_link_libraries(glcore
    PUBLIC GLEW::GLEW glfw glm::glm OpenGL::GL third_party::imgui
    PRIVATE glcore_warnings third_party::gli)

target_compile_definitions(glcore PUBLIC
    GLC_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
    GLC_GL_MAJOR=${GLCORE_GL_MAJOR}
    GLC_GL_MINOR=${GLCORE_GL_MINOR}
    # Enables GLC_CHECK and the "is this program bound?" uniform assertion.
    $<$<CONFIG:Debug>:GLC_DEBUG>
    # macOS marks the whole GL API deprecated.
    $<$<PLATFORM_ID:Darwin>:GL_SILENCE_DEPRECATION>)
