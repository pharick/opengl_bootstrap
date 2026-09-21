#version 330

// The texture is the colour -- the book's Tex.frag.
//
// After a chapter of textures that were not pictures, this one is: whatever
// the fetch returns is the fragment. There is no light, no material and no
// gamma correction between the texel and the screen, so what shows up is
// exactly what the sampler produced -- which is the point, because the rest
// of the chapter is about what the sampler does to it.

in vec2 colorCoord;

uniform sampler2D colorTexture;

out vec4 outputColor;

void main() {
	outputColor = texture(colorTexture, colorCoord);
}
