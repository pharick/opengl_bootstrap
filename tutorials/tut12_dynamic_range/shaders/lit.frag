#version 330

// The whole lighting model, once. Both vertex shaders hand over the same three
// varyings, so this file never learns whether the diffuse colour came from a
// vertex attribute or from the material block -- which is what keeps it to one
// file instead of the book's four.

#include <common/specular.glsl>

layout(std140) uniform;

in vec4 diffuseColor;
in vec3 vertexNormal;
in vec3 cameraSpacePosition;

out vec4 outputColor;

struct PerLight {
	vec4 cameraSpaceLightPos;
	vec4 lightIntensity;
};

const int numberOfLights = 4;

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

float calcAttenuation(in vec3 surfacePosition, in vec3 lightPosition, out vec3 lightDirection) {
	vec3 lightDifference = lightPosition - surfacePosition;
	float lightDistanceSquared = dot(lightDifference, lightDifference);
	lightDirection = lightDifference * inversesqrt(lightDistanceSquared);
	return 1.0 / (1.0 + Lgt.lightAttenuation * lightDistanceSquared);
}

vec4 computeLighting(in PerLight light, in vec3 surfaceNormal, in vec3 dirToViewer) {
	vec3 lightDirection;
	vec4 lightIntensity;

	// w discriminates the two kinds. A direction gets no attenuation -- the sun
	// does not get dimmer across a hillside.
	if (light.cameraSpaceLightPos.w == 0.0) {
		lightDirection = normalize(light.cameraSpaceLightPos.xyz);
		lightIntensity = light.lightIntensity;
	} else {
		float attenuation =
		    calcAttenuation(cameraSpacePosition, light.cameraSpaceLightPos.xyz, lightDirection);
		lightIntensity = attenuation * light.lightIntensity;
	}

	float cosAngIncidence = clamp(dot(surfaceNormal, lightDirection), 0.0, 1.0);

	// Gaussian, not Phong: specularShininess is an angular width in radians,
	// which is why the material values are 0.05..0.6 rather than exponents in
	// the tens.
	float specularTerm =
	    gaussianTerm(surfaceNormal, lightDirection, dirToViewer, Mtl.specularShininess);

	// Same guard as tut11. Without it a surface facing away from a lamp can
	// still line its reflection up with the eye, producing a highlight on the
	// dark side where the diffuse term is exactly zero.
	specularTerm = cosAngIncidence > 0.0 ? specularTerm : 0.0;

	return (diffuseColor * lightIntensity * cosAngIncidence) +
	       (Mtl.specularColor * lightIntensity * specularTerm);
}

void main() {
	// Interpolating two unit vectors does not produce a unit vector, so the
	// normal is renormalized here rather than in the vertex shader.
	vec3 surfaceNormal = normalize(vertexNormal);

	// Camera space puts the eye at the origin, so the direction to it is free.
	vec3 dirToViewer = normalize(-cameraSpacePosition);

	vec4 accumLighting = diffuseColor * Lgt.ambientIntensity;
	for (int light = 0; light < numberOfLights; ++light) {
		accumLighting += computeLighting(Lgt.lights[light], surfaceNormal, dirToViewer);
	}

	outputColor = accumLighting;
}
