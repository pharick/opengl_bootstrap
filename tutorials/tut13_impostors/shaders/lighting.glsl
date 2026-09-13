// The lighting model for this chapter, shared by every lit fragment shader.
//
// The book has it in each of them, with the only difference being where the
// surface position and normal come from: a vertex shader in one case, a
// per-fragment calculation in the other. The model itself does not care, so
// it lives here and takes both as arguments.
//
// The material is an argument too, rather than a block read from inside, so
// that the same code works whether the shader has one material bound
// (material.glsl) or picks one out of an array per primitive
// (impostor_geom.frag). The Light block is declared here since nothing varies
// about it.

#ifndef TUT13_LIGHTING_GLSL
#define TUT13_LIGHTING_GLSL

#include <common/specular.glsl>

layout(std140) uniform;

/// One material, as both the single-material block and the array block hold
/// it. std140 pads the trailing float to 16 bytes, so the C++ mirror is 48
/// bytes and so is an array element.
struct MaterialEntry {
	vec4 diffuseColor;
	vec4 specularColor;
	float specularShininess;
};

struct PerLight {
	vec4 cameraSpaceLightPos;
	vec4 lightIntensity;
};

const int numberOfLights = 2;

uniform Light {
	vec4 ambientIntensity;
	float lightAttenuation;
	PerLight lights[numberOfLights];
}
Lgt;

float calcAttenuation(in vec3 cameraSpacePosition, in vec3 cameraSpaceLightPos,
                      out vec3 lightDirection) {
	vec3 lightDifference = cameraSpaceLightPos - cameraSpacePosition;
	float lightDistanceSquared = dot(lightDifference, lightDifference);
	lightDirection = lightDifference * inversesqrt(lightDistanceSquared);
	return 1.0 / (1.0 + Lgt.lightAttenuation * lightDistanceSquared);
}

/// Diffuse plus Gaussian specular from one light, at a camera-space point with
/// a unit-length camera-space normal. Same model as tut12, minus tone mapping.
vec4 computeLighting(in PerLight light, in MaterialEntry material, in vec3 cameraSpacePosition,
                     in vec3 surfaceNormal) {
	vec3 lightDirection;
	vec4 lightIntensity;

	// w discriminates the two kinds: a direction gets no attenuation.
	if (light.cameraSpaceLightPos.w == 0.0) {
		lightDirection = normalize(light.cameraSpaceLightPos.xyz);
		lightIntensity = light.lightIntensity;
	} else {
		float attenuation =
		    calcAttenuation(cameraSpacePosition, light.cameraSpaceLightPos.xyz, lightDirection);
		lightIntensity = attenuation * light.lightIntensity;
	}

	float cosAngIncidence = clamp(dot(surfaceNormal, lightDirection), 0.0, 1.0);

	// Camera space puts the eye at the origin, so the direction to it is free.
	vec3 dirToViewer = normalize(-cameraSpacePosition);

	float specularTerm =
	    gaussianTerm(surfaceNormal, lightDirection, dirToViewer, material.specularShininess);

	// No highlight on a surface that faces away from the lamp.
	specularTerm = cosAngIncidence > 0.0 ? specularTerm : 0.0;

	return (material.diffuseColor * lightIntensity * cosAngIncidence) +
	    (material.specularColor * lightIntensity * specularTerm);
}

/// Ambient plus every light in the block. `surfaceNormal` must be unit length;
/// the caller knows whether it came from interpolation (and needs normalizing)
/// or was constructed on the sphere (and does not).
vec4 accumulateLighting(in MaterialEntry material, in vec3 cameraSpacePosition,
                        in vec3 surfaceNormal) {
	vec4 accumLighting = material.diffuseColor * Lgt.ambientIntensity;
	for (int light = 0; light < numberOfLights; ++light) {
		accumLighting +=
		    computeLighting(Lgt.lights[light], material, cameraSpacePosition, surfaceNormal);
	}
	return accumLighting;
}

/// The book's shortcut: sqrt is pow(x, 1/2), a gamma of 2.0 rather than 2.2.
/// Close enough for a chapter that is not about gamma, and cheaper than pow.
vec4 gammaCorrect(in vec4 linear) {
	return sqrt(linear);
}

#endif
