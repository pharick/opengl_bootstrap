#version 330

// The book's projLight.frag: Tutorial 16's lighting, plus a spotlight whose
// intensity is read from a texture projected onto the scene.
//
// The projected light is an ordinary point light as far as computeLighting is
// concerned. Its position is a uniform, so it attenuates and has a direction
// like any other; only its intensity is unusual, varying across the scene
// with where each fragment lands in the flashlight's texture.

in vec2 colorCoord;
in vec3 cameraSpacePosition;
in vec3 cameraSpaceNormal;
in vec4 lightProjPosition;

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
uniform sampler2D lightProjTex;

uniform vec3 cameraSpaceProjLightPos;

/// The texture stores [0, 1]; the scene's lights are HDR intensities.
const float projLightScale = 4.0;

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

	// textureProj divides .xy by the last component -- here w, so .xyw. Z is
	// never used, which is why the light's zNear and zFar do not matter.
	PerLight projLight;
	projLight.cameraSpaceLightPos = vec4(cameraSpaceProjLightPos, 1.0);
	projLight.lightIntensity =
	    textureProj(lightProjTex, lightProjPosition.xyw) * projLightScale;

	// The divide does not care which side of the light a point is on, so
	// without this the texture would also be projected out of the back of
	// the flashlight, upside down. w is the light-space -z: positive in front.
	projLight.lightIntensity = lightProjPosition.w > 0.0 ? projLight.lightIntensity : vec4(0.0);

	vec4 accumLighting = diffuseColor * Lgt.ambientIntensity;
	for (int light = 0; light < numberOfLights; ++light) {
		accumLighting += computeLighting(diffuseColor, Lgt.lights[light]);
	}
	accumLighting += computeLighting(diffuseColor, projLight);

	outputColor = accumLighting / Lgt.maxIntensity;
}
