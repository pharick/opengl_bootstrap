#version 330

// Shininess from a texture, Gaussian from the table.
//
// The chapter's point. A material parameter that used to be one number per
// mesh is now one number per texel, and the mesh's texture coordinates say
// which texel each fragment gets. Nothing about the lighting changed -- the
// shininess just stopped being a constant.

#include "lighting.glsl"

in vec3 vertexNormal;
in vec3 cameraSpacePosition;
in vec2 shinTexCoord;

out vec4 outputColor;

// Two textures, two samplers in GLSL, one sampler *object* on the C++ side:
// both units are read with nearest filtering and clamped edges, so the
// application binds the same sampler object to both.
uniform sampler2D gaussianTexture;
uniform sampler2D shininessTexture;

float specularTerm(in float cosAngNormalHalf, in float specularShininess) {
	vec2 texCoord = vec2(cosAngNormalHalf, specularShininess);
	return texture(gaussianTexture, texCoord).r;
}

void main() {
	vec3 surfaceNormal = normalize(vertexNormal);

	// This texture *is* a picture -- someone painted the smudges -- but the
	// shader does not treat it as one. The red channel comes back as a float
	// on [0, 1] and is used as a width in radians. The bright smudges are the
	// wide, dull highlights; the dark ground is the tight, shiny one.
	//
	// The fetch happens once, before the light loop: the shininess is a
	// property of the surface point, not of any particular light.
	float specularShininess = texture(shininessTexture, shinTexCoord).r;

	outputColor =
	    gammaCorrect(accumulateLighting(cameraSpacePosition, surfaceNormal, specularShininess));
}
