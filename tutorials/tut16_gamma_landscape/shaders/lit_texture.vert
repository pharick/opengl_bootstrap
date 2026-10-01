#version 330

// Position, normal, texture coordinate -- the book's PNT.vert.
//
// Lighting happens per fragment, in camera space, so this hands over the
// camera-space position and normal along with the coordinate. The normal goes
// through the model-to-camera matrix itself rather than a normal matrix: the
// terrain is only rotated, and a rotation is its own inverse-transpose.

layout(std140) uniform;

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec3 inNormal;
layout(location = 5) in vec2 inTexCoord;

out vec2 colorCoord;
out vec3 cameraSpacePosition;
out vec3 cameraSpaceNormal;

uniform Projection {
	mat4 cameraToClipMatrix;
};

uniform mat4 modelToCameraMatrix;

void main() {
	cameraSpacePosition = (modelToCameraMatrix * vec4(inPosition, 1.0)).xyz;
	gl_Position = cameraToClipMatrix * vec4(cameraSpacePosition, 1.0);
	cameraSpaceNormal = (modelToCameraMatrix * vec4(inNormal, 0.0)).xyz;
	colorCoord = inTexCoord;
}
