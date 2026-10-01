#version 330

// Position and nothing else. Used for the light markers, which must not be lit
// by the lights they are standing in for.

layout(std140) uniform;

layout(location = 0) in vec3 inPosition;

uniform Projection {
	mat4 cameraToClipMatrix;
};

uniform mat4 modelToCameraMatrix;

void main() {
	gl_Position = cameraToClipMatrix * (modelToCameraMatrix * vec4(inPosition, 1.0));
}
