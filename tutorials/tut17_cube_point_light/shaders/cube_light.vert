#version 330

// The book's cubeLight.vert: litTexture.vert plus the position in the light's
// own space.
//
// cameraToLightMatrix takes a view-camera-space position back to world space
// and then into the light's space, where the light sits at the origin with
// its own orientation. There is no projection: a cube map is indexed by a
// direction, and the direction from the light to the point is just the
// point's position in that space.

layout(std140) uniform;

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec3 inNormal;
layout(location = 5) in vec2 inTexCoord;

out vec2 colorCoord;
out vec3 cameraSpacePosition;
out vec3 cameraSpaceNormal;
out vec3 lightSpacePosition;

uniform Projection {
	mat4 cameraToClipMatrix;
};

uniform mat4 modelToCameraMatrix;
uniform mat3 normalModelToCameraMatrix;
uniform mat4 cameraToLightMatrix;

void main() {
	cameraSpacePosition = (modelToCameraMatrix * vec4(inPosition, 1.0)).xyz;
	gl_Position = cameraToClipMatrix * vec4(cameraSpacePosition, 1.0);
	cameraSpaceNormal = normalize(normalModelToCameraMatrix * inNormal);
	lightSpacePosition = (cameraToLightMatrix * vec4(cameraSpacePosition, 1.0)).xyz;
	colorCoord = inTexCoord;
}
