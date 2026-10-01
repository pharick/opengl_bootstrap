#version 330

// The texel, written out unchanged -- the book's textureNoGamma.frag.
//
// Whether that is right depends entirely on the texture. From a GL_RGB8
// texture the value is the byte over 255, still gamma-encoded, and the
// display shows the image as authored. From a GL_SRGB8 texture it has been
// decoded to linear, and writing linear values to a display that expects
// encoded ones darkens every mid-tone.

in vec2 colorCoord;

uniform sampler2D colorTexture;

out vec4 outputColor;

void main() {
	outputColor = texture(colorTexture, colorCoord);
}
