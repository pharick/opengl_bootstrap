# add_tutorial(<name>)
#
# Builds tutorials/<name>/**.cpp into an executable named <name>, linked against
# glcore. Each tutorial is fully independent -- adding a chapter means creating
# the directory and adding one line to tutorials/CMakeLists.txt.
#
# GLC_TUTORIAL_DIR is injected so the tutorial can locate its own shaders no
# matter what the working directory is, and so the hot-reloader watches the
# *source* files you are editing rather than a stale copy in build/.

function(add_tutorial name)
    file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/${name}/*.cpp")

    if(NOT _sources)
        message(FATAL_ERROR "add_tutorial(${name}): no .cpp files under ${CMAKE_CURRENT_SOURCE_DIR}/${name}")
    endif()

    add_executable(${name} ${_sources})
    target_link_libraries(${name} PRIVATE glcore::glcore glcore_warnings)
    target_compile_definitions(${name} PRIVATE
        GLC_TUTORIAL_DIR="${CMAKE_CURRENT_SOURCE_DIR}/${name}")
    set_target_properties(${name} PROPERTIES FOLDER tutorials)

    if(GLCORE_TIDY_COMMAND)
        set_target_properties(${name} PROPERTIES CXX_CLANG_TIDY "${GLCORE_TIDY_COMMAND}")
    endif()
endfunction()
