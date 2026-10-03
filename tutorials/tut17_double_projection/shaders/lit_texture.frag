#version 330

// Diffuse lighting of a texture -- the book's litTexture.frag, unchanged from
// Tutorial 16.
//
// Every value in here is linear. The texture is GL_SRGB8_ALPHA8, so the fetch
// has already decoded it; the lights are physical intensities that add; and
// the result is divided by the scene's maximum intensity and written out
// as-is. There is no pow() anywhere: the encoding for the screen happens after
// this shader, in the sRGB framebuffer.

in vec2 colorCoord;
in vec3 cameraSpacePosition;
in vec3 cameraSpaceNormal;

out vec4 outputColor;

layout(std140) uniform;

struct PerLight {
	vec4 cameraSpaceLightPos;
	vec4 lightIntensity;
};

const int maxNumberOfLights = 4;

uniform Light {
	vec4 ambientIntensity;
	float lightAttenuation;
	float maxIntensity;
	PerLight lights[maxNumberOfLights];
}
Lgt;

/// How many entries of Lgt.lights are live.
uniform int numberOfLights;

uniform sampler2D diffuseColorTex;

float calcAttenuation(in vec3 lightPosition, out vec3 lightDirection) {
	vec3 lightDifference = lightPosition - cameraSpacePosition;
	float lightDistanceSquared = dot(lightDifference, lightDifference);
	lightDirection = lightDifference * inversesqrt(lightDistanceSquared);
	return 1.0 / (1.0 + Lgt.lightAttenuation * lightDistanceSquared);
}

vec4 computeLighting(in vec4 diffuseColor, in PerLight light) {
	vec3 lightDirection;
	vec4 lightIntensity;

	// w = 0 is the sun: a direction, and no falloff with distance.
	if (light.cameraSpaceLightPos.w == 0.0) {
		lightDirection = vec3(light.cameraSpaceLightPos);
		lightIntensity = light.lightIntensity;
	} else {
		float attenuation = calcAttenuation(light.cameraSpaceLightPos.xyz, lightDirection);
		lightIntensity = attenuation * light.lightIntensity;
	}

	vec3 surfaceNormal = normalize(cameraSpaceNormal);
	float cosAngIncidence = clamp(dot(surfaceNormal, lightDirection), 0.0, 1.0);

	return diffuseColor * lightIntensity * cosAngIncidence;
}

void main() {
	vec4 diffuseColor = texture(diffuseColorTex, colorCoord);

	vec4 accumLighting = diffuseColor * Lgt.ambientIntensity;
	for (int light = 0; light < numberOfLights; ++light) {
		accumLighting += computeLighting(diffuseColor, Lgt.lights[light]);
	}

	outputColor = accumLighting / Lgt.maxIntensity;
}
