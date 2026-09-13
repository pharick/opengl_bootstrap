// The lighting model for this chapter, shared by mesh.frag and impostor.frag.
//
// The book has it twice (Lighting.frag and BasicImpostor.frag), with the only
// difference being where the surface position and normal come from: a vertex
// shader in one case, a per-fragment calculation in the other. The model itself
// does not care, so it lives here and takes both as arguments.
//
// The uniform blocks are declared here too, since the functions read them.
// Including this file therefore commits a shader to the Light and Material
// block layouts that main.cpp uploads.

#ifndef TUT13_LIGHTING_GLSL
#define TUT13_LIGHTING_GLSL

#include <common/specular.glsl>

layout(std140) uniform;

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

uniform Material {
	vec4 diffuseColor;
	vec4 specularColor;
	float specularShininess;
}
Mtl;

float calcAttenuation(in vec3 cameraSpacePosition, in vec3 cameraSpaceLightPos,
                      out vec3 lightDirection) {
	vec3 lightDifference = cameraSpaceLightPos - cameraSpacePosition;
	float lightDistanceSquared = dot(lightDifference, lightDifference);
	lightDirection = lightDifference * inversesqrt(lightDistanceSquared);
	return 1.0 / (1.0 + Lgt.lightAttenuation * lightDistanceSquared);
}

/// Diffuse plus Gaussian specular from one light, at a camera-space point with
/// a unit-length camera-space normal. Same model as tut12, minus tone mapping.
vec4 computeLighting(in PerLight light, in vec3 cameraSpacePosition, in vec3 surfaceNormal) {
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
	    gaussianTerm(surfaceNormal, lightDirection, dirToViewer, Mtl.specularShininess);

	// No highlight on a surface that faces away from the lamp.
	specularTerm = cosAngIncidence > 0.0 ? specularTerm : 0.0;

	return (Mtl.diffuseColor * lightIntensity * cosAngIncidence) +
	    (Mtl.specularColor * lightIntensity * specularTerm);
}

/// Ambient plus every light in the block. `surfaceNormal` must be unit length;
/// the caller knows whether it came from interpolation (and needs normalizing)
/// or was constructed on the unit sphere (and does not).
vec4 accumulateLighting(in vec3 cameraSpacePosition, in vec3 surfaceNormal) {
	vec4 accumLighting = Mtl.diffuseColor * Lgt.ambientIntensity;
	for (int light = 0; light < numberOfLights; ++light) {
		accumLighting += computeLighting(Lgt.lights[light], cameraSpacePosition, surfaceNormal);
	}
	return accumLighting;
}

/// The book's shortcut: sqrt is pow(x, 1/2), a gamma of 2.0 rather than 2.2.
/// Close enough for a chapter that is not about gamma, and cheaper than pow.
vec4 gammaCorrect(in vec4 linear) {
	return sqrt(linear);
}

#endif
