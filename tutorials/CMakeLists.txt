# One executable per gltut chapter. Add a directory, add a line.
add_tutorial(tut01_hello_triangle)

# Needs the XML mesh loader, so it is only built when tinyxml2 is available.
if(GLCORE_WITH_MESH_XML)
    add_tutorial(tut02_cube_and_camera)
endif()

add_tutorial(tut03_textured_quad)
add_tutorial(tut09_basic_lighting)
