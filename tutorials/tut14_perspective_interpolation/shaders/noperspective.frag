#version 330

noperspective in vec4 vertexColor;

out vec4 outputColor;

void main() {
	outputColor = vertexColor;
}
