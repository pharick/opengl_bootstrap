#version 330

// Resolved by glcore against assets/shaders before the driver sees this file.
#include <common/gamma.glsl>

in vec2 texCoord;

uniform sampler2D diffuse;
uniform float tiling;
uniform bool manualGamma;

out vec4 outColor;

void main() {
	vec3 color = texture(diffuse, texCoord * tiling).rgb;
	if (manualGamma) {
		color = linearToSrgb(color);
	}
	outColor = vec4(color, 1.0);
}
