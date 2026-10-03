#version 330

// Position and a per-vertex colour, unlit. Used for the cube light's axes.

layout(std140) uniform;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

out vec4 objectColor;

uniform Projection {
	mat4 cameraToClipMatrix;
};

uniform mat4 modelToCameraMatrix;

void main() {
	gl_Position = cameraToClipMatrix * (modelToCameraMatrix * vec4(inPosition, 1.0));
	objectColor = inColor;
}
