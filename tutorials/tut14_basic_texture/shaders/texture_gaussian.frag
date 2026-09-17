#version 330

// The same highlight, read out of a table instead of computed.
//
// The texture is a single row of 8-bit normalized integers holding that same
// Gaussian sampled across the whole range of its argument, so the acos and the
// exp collapse into one fetch. Nothing in it is a colour and nothing ever looks
// at it: it is a function that happens to live in texture memory.

#include "lighting.glsl"

in vec3 vertexNormal;
in vec3 cameraSpacePosition;

out vec4 outputColor;

// A sampler is not a value -- it has no contents you can read, add or assign,
// and it cannot be a local, a struct member or a uniform block member. Treat it
// as a hook: the application names a texture image unit in this uniform, binds
// a texture to that unit, and the only thing the shader can do with it is hand
// it to a built-in like texture().
uniform sampler1D gaussianTexture;

float specularTerm(in float cosAngNormalHalf) {
	// 1D texture coordinates are normalized: 0 is the start of the table and 1
	// the end, whatever its length. That is what lets the 1-4 keys swap in a
	// table of a different size without the shader knowing or caring.
	//
	// The fetch returns a vec4 even though the texture holds a single channel;
	// the other three components read as 0, 0, 1. And what comes back is a
	// float, because sampler1D is a floating-point sampler -- the normalized
	// byte in the texture is divided by 255 on the way out.
	return texture(gaussianTexture, cosAngNormalHalf).r;
}

void main() {
	vec3 surfaceNormal = normalize(vertexNormal);

	outputColor = gammaCorrect(accumulateLighting(cameraSpacePosition, surfaceNormal));
}
