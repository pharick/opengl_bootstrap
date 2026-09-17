#version 330

// Position and normal -- the book's PN.vert, unchanged from Tutorial 13.
//
// Worth noticing for a chapter about textures: nothing here mentions one. The
// look-up coordinate is not a vertex attribute and is not interpolated; it is
// computed per fragment out of vectors the lighting model already had.

layout(std140) uniform;

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec3 inNormal;

out vec3 vertexNormal;
out vec3 cameraSpacePosition;

uniform Projection {
	mat4 cameraToClipMatrix;
};

uniform mat4 modelToCameraMatrix;
uniform mat3 normalModelToCameraMatrix;

void main() {
	vec4 cameraPosition = modelToCameraMatrix * vec4(inPosition, 1.0);
	gl_Position = cameraToClipMatrix * cameraPosition;

	vertexNormal = normalModelToCameraMatrix * inNormal;
	cameraSpacePosition = vec3(cameraPosition);
}
