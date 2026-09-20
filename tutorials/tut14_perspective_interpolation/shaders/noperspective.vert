#version 330

// The same vertex shader with one word changed -- the book's
// NoCorrectVertexColors.vert.
//
// `noperspective` asks for plain linear interpolation in window space: the
// rasterizer ignores W and lerps the color across the pixels of the triangle
// as if it were a flat picture. That is wrong for anything with depth, and it
// is wrong in a very particular way: the two triangles of a quad no longer
// agree along their shared diagonal, and the far colors reach toward the
// camera. It is also, as the faux hallway shows, all that perspective-correct
// interpolation can manage when W has nothing to say.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

noperspective out vec4 vertexColor;

uniform mat4 cameraToClipMatrix;

void main() {
	gl_Position = cameraToClipMatrix * vec4(inPosition, 1.0);
	vertexColor = inColor;
}
