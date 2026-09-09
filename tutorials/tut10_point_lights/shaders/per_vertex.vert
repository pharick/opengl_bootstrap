#version 330

#include <common/lighting.glsl>

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec3 inNormal;

uniform mat4 modelToCamera;

uniform vec3 diffuseColor;
uniform vec3 lightPosition;
uniform vec3 lightIntensity;
uniform vec3 ambientIntensity;
uniform mat3 normalModelToCamera;
uniform float lightAttenuation;
uniform bool useInverseSquaredAttenuation;

layout(std140) uniform Projection {
	mat4 cameraToClip;
};

out vec3 vertexColor;

void main() {
	vec4 cameraPosition = modelToCamera * vec4(inPosition, 1.0);
	gl_Position = cameraToClip * cameraPosition;

	vec3 normalCamera = normalize(normalModelToCamera * inNormal);
	vec3 lightDirection;
	vec3 attenuatedLightIntensity =
	    ApplyLightIntensity(cameraPosition.xyz, lightPosition, lightIntensity, lightAttenuation,
		                    useInverseSquaredAttenuation, lightDirection);
	float cosAngleIncidence = dot(normalCamera, lightDirection);
	vertexColor = diffuseColor * attenuatedLightIntensity * clamp(cosAngleIncidence, 0.0, 1.0) +
	              diffuseColor * ambientIntensity;
}
