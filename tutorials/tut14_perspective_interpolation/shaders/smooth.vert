#version 330

// Position and color, straight through -- the book's SmoothVertexColors.vert.
//
// `smooth` is OpenGL's default and every earlier chapter has relied on it
// without writing it. It is spelled out here because the qualifier is the
// whole subject of this program: it asks the rasterizer to interpolate the
// value in a space that is linear with respect to clip space, not in window
// space. The rasterizer can only do that because gl_Position still carries
// clip-space W -- which is the last of the reasons the vertex shader hands
// over clip space instead of dividing by W itself.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

smooth out vec4 vertexColor;

uniform mat4 cameraToClipMatrix;

void main() {
	gl_Position = cameraToClipMatrix * vec4(inPosition, 1.0);
	vertexColor = inColor;
}
