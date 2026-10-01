#version 330

// The texel, gamma-corrected on the way out -- the book's textureGamma.frag.
//
// The inverse of the other shader's problem. From a GL_SRGB8 texture the
// value is linear, and encoding it here restores the image exactly. From a
// GL_RGB8 texture it was never decoded, so this encodes it a second time and
// everything but black and white drifts towards white.
//
// Alpha is not a colour and is left alone.

#include <common/gamma.glsl>

in vec2 colorCoord;

uniform sampler2D colorTexture;

out vec4 outputColor;

void main() {
	vec4 color = texture(colorTexture, colorCoord);
	outputColor = vec4(linearToSrgb(color.rgb), color.a);
}
