#version 330

// Shininess from a texture, Gaussian computed per fragment.
//
// The reference for the textured modes. On the flat plane the difference from
// texture_shininess.frag is stark: the table shows concentric rings around
// every highlight even at 512 columns, because a flat surface spreads a
// highlight over far more pixels than the curved infinity symbol does, and
// every one of those pixels shows which texel it snapped to.

#include "lighting.glsl"

in vec3 vertexNormal;
in vec3 cameraSpacePosition;
in vec2 shinTexCoord;

out vec4 outputColor;

// No gaussianTexture here. The application still binds one to unit 0, which
// is harmless: nothing in this program points at that unit.
uniform sampler2D shininessTexture;

// Tutorial 11's Gaussian, with the shininess as an argument rather than a
// uniform. Same acos, divide, square and exp as before, per light per
// fragment.
float specularTerm(in float cosAngNormalHalf, in float specularShininess) {
	float exponent = acos(cosAngNormalHalf) / specularShininess;
	return exp(-(exponent * exponent));
}

void main() {
	vec3 surfaceNormal = normalize(vertexNormal);

	float specularShininess = texture(shininessTexture, shinTexCoord).r;

	outputColor =
	    gammaCorrect(accumulateLighting(cameraSpacePosition, surfaceNormal, specularShininess));
}
