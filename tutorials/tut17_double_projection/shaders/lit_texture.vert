#version 330

// Position, normal, texture coordinate -- the book's litTexture.vert.
//
// Unlike the terrain in Tutorial 16, these objects are scaled, so normals get
// their own matrix: the inverse-transpose of the model-to-camera matrix.
//
// Nothing in here knows there are two cameras. cameraToClipMatrix is the
// whole difference between the left and right views: for the right one it has
// a rotation multiplied onto the end of it, after the projection.

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
uniform mat3 normalModelToCameraMatrix;

void main() {
	cameraSpacePosition = (modelToCameraMatrix * vec4(inPosition, 1.0)).xyz;
	gl_Position = cameraToClipMatrix * vec4(cameraSpacePosition, 1.0);
	cameraSpaceNormal = normalize(normalModelToCameraMatrix * inNormal);
	colorCoord = inTexCoord;
}
