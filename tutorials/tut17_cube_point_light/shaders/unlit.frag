#version 330

// One flat colour, straight out. Used for the orbit-point marker.

uniform vec4 objectColor;

out vec4 outputColor;

void main() {
	outputColor = objectColor;
}
