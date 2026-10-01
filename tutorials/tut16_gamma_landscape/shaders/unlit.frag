#version 330

// One flat colour, straight out. The markers are set to the intensity of the
// light they represent, so a lamp that has faded to black disappears with it.

uniform vec4 objectColor;

out vec4 outputColor;

void main() {
	outputColor = objectColor;
}
