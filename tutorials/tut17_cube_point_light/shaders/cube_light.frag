#version 330

// The book's cubeLight.frag: Tutorial 16's lighting, plus a point light whose
// intensity depends on the direction it shines in.
//
// The intensity comes from a cube map, sampled with the direction from the
// light to the fragment. The hardware picks the face from the direction's
// largest component and the texel from the other two divided by it, so the
// vector's length does not matter -- the normalize is the book's, and is only
// there for readability. Unlike the projected light there is no divide, no
// w > 0 test and no border: every direction lands somewhere on the cube.

in vec2 colorCoord;
in vec3 cameraSpacePosition;
in vec3 cameraSpaceNormal;
in vec3 lightSpacePosition;

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

/// How many entries of Lgt.lights are live. Zero when G turns them off.
uniform int numberOfLights;

uniform sampler2D diffuseColorTex;
uniform samplerCube lightCubeTex;

uniform vec3 cameraSpaceCubeLightPos;

/// The texture stores [0, 1]; the scene's lights are HDR intensities.
const float cubeLightScale = 6.0;

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

	PerLight cubeLight;
	cubeLight.cameraSpaceLightPos = vec4(cameraSpaceCubeLightPos, 1.0);
	vec3 directionFromLight = normalize(lightSpacePosition);
	cubeLight.lightIntensity = texture(lightCubeTex, directionFromLight) * cubeLightScale;

	vec4 accumLighting = diffuseColor * Lgt.ambientIntensity;
	for (int light = 0; light < numberOfLights; ++light) {
		accumLighting += computeLighting(diffuseColor, Lgt.lights[light]);
	}
	accumLighting += computeLighting(diffuseColor, cubeLight);

	outputColor = accumLighting / Lgt.maxIntensity;
}
