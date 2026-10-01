#version 330

// The texel, gamma-corrected on the way out -- the book's textureGamma.frag.
//
// The texture decoded to linear on fetch; this encodes back. Between the two
// the filtering ran on linear values, so the only thing left that can be
// wrong is what the mipmap levels themselves contain.
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
