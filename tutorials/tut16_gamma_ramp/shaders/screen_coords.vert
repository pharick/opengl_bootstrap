#version 330

// Window pixels in, clip space out -- the book's screenCoords.vert.
//
// The "camera to clip" matrix here is not a perspective at all: it maps
// pixels measured from the top-left corner onto [-1, 1], with Y flipped. Two
// unsigned shorts per position, converted to float unnormalized, so a vertex
// at pixel 90 arrives as 90.0.

layout(std140) uniform;

layout(location = 0) in vec2 inPosition;
layout(location = 5) in vec2 inTexCoord;

out vec2 colorCoord;

uniform Projection {
	mat4 cameraToClipMatrix;
};

void main() {
	gl_Position = cameraToClipMatrix * vec4(inPosition, 0.0, 1.0);
	colorCoord = inTexCoord;
}
