#version 330

// The reference: the Gaussian evaluated per fragment, every fragment.
//
// This is the expensive one the chapter sets out to replace. Per light, per
// fragment: an inverse cosine, a divide, a square and an exponential. Blinn-
// Phong would have cost one pow() for a worse-looking highlight.
//
// Press Spacebar for texture_gaussian.frag and look for the difference.

#include "lighting.glsl"

in vec3 vertexNormal;
in vec3 cameraSpacePosition;

out vec4 outputColor;

// The same curve as gaussianTerm() in common/specular.glsl, written out in the
// one argument this chapter cares about: the shininess is a uniform, and the
// normal and half-angle vector have already been collapsed into their dot
// product by the caller.
float specularTerm(in float cosAngNormalHalf) {
	float exponent = acos(cosAngNormalHalf) / Mtl.specularShininess;
	return exp(-(exponent * exponent));
}

void main() {
	// Interpolating two unit vectors does not produce a unit vector.
	vec3 surfaceNormal = normalize(vertexNormal);

	outputColor = gammaCorrect(accumulateLighting(cameraSpacePosition, surfaceNormal));
}
