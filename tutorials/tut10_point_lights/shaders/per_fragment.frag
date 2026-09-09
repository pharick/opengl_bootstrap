#version 330

#include <common/lighting.glsl>

uniform vec3 diffuseColor;
uniform vec3 lightPosition;
uniform vec3 lightIntensity;
uniform vec3 ambientIntensity;
uniform float lightAttenuation;
uniform bool useInverseSquaredAttenuation;

in vec3 cameraSpacePosition;
in vec3 cameraSpaceNormal;

out vec4 outColor;

void main() {
	vec3 normalizedNormal = normalize(cameraSpaceNormal);
	vec3 lightDirection;
	vec3 attenuatedLightIntensity =
	    ApplyLightIntensity(cameraSpacePosition, lightPosition, lightIntensity, lightAttenuation,
		                    useInverseSquaredAttenuation, lightDirection);
	float diffuseFactor = clamp(dot(normalizedNormal, lightDirection), 0.0, 1.0);
	vec3 diffuse = diffuseColor * attenuatedLightIntensity * diffuseFactor;
	vec3 ambient = ambientIntensity * diffuseColor;
	vec3 finalColor = diffuse + ambient;
	outColor = vec4(finalColor, 1.0);
}
