#version 330

// Position and nothing else, for the light marker.

layout(std140) uniform;

layout(location = 0) in vec3 inPosition;

uniform Projection {
	mat4 cameraToClipMatrix;
};

uniform mat4 modelToCameraMatrix;

void main() {
	gl_Position = cameraToClipMatrix * (modelToCameraMatrix * vec4(inPosition, 1.0));
}
