#version 330

// The texel, written out unchanged -- the book's textureNoGamma.frag.
//
// Both checkerboards are GL_SRGB8, so what arrives here is linear, and a
// linear value written to a display that expects an encoded one comes out
// too dark. The darkness happens to cancel the badly-averaged mipmaps of
// checker_linear.dds, which is why that file looked fine for so long.

in vec2 colorCoord;

uniform sampler2D colorTexture;

out vec4 outputColor;

void main() {
	outputColor = texture(colorTexture, colorCoord);
}
