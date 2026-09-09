#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec3 inNormal;

uniform mat4 modelToCamera;
uniform mat3 normalModelToCamera;

layout(std140) uniform Projection {
	mat4 cameraToClip;
};

out vec3 cameraSpacePosition;
out vec3 cameraSpaceNormal;

void main() {
	vec4 cameraPosition = modelToCamera * vec4(inPosition, 1.0);
	gl_Position = cameraToClip * cameraPosition;

	cameraSpacePosition = cameraPosition.xyz;
	cameraSpaceNormal = normalModelToCamera * inNormal;
}
