#version 330

// Position and texture coordinate -- the book's PT.vert.
//
// No normal, no lighting: this chapter is about reading a picture, and the
// vertex shader's only job for the picture is to pass the coordinate along.
// It reaches the fragment shader perspective-correctly interpolated, which on
// a plane receding to the horizon is the whole difference between a
// checkerboard and a mess.
//
// Attribute 5 is gltut's slot for texture coordinates; BigPlane.xml and
// Corridor.xml agree.

layout(std140) uniform;

layout(location = 0) in vec3 inPosition;
layout(location = 5) in vec2 inTexCoord;

out vec2 colorCoord;

uniform Projection {
	mat4 cameraToClipMatrix;
};

uniform mat4 modelToCameraMatrix;

void main() {
	gl_Position = cameraToClipMatrix * (modelToCameraMatrix * vec4(inPosition, 1.0));
	colorCoord = inTexCoord;
}
