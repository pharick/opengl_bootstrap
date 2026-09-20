#version 330

// Position and normal -- the book's PN.vert, used by the fixed-shininess mode.
//
// This is the mesh as every earlier chapter saw it. The file also carries
// texture coordinates, but the "lit" VAO this mode draws through does not
// source them, and nothing here would know what to do with them.

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
