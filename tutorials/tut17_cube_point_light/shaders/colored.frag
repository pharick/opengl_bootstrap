#version 330

// The interpolated vertex colour, straight out.

in vec4 objectColor;

out vec4 outputColor;

void main() {
	outputColor = objectColor;
}
