#version 330

// The interpolated color, straight out. The qualifier on a fragment input has
// to match the one on the vertex output feeding it.

smooth in vec4 vertexColor;

out vec4 outputColor;

void main() {
	outputColor = vertexColor;
}
