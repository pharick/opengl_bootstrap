#version 330

// For meshes that carry per-vertex colours: the "lit-color" VAO, plus Ground,
// whose default VAO supplies all three attributes.
//
// The vertex colour is the diffuse colour, used as-is. The material block is
// not read here at all -- for these meshes it contributes specular only, which
// is why the material table's diffuse values for the tetrahedron, cube and
// cylinder never reach a shader. Multiplying the two would halve them.
//
// The fragment shader consumes the same three varyings either way, so it never
// learns which vertex shader produced them.

layout(std140) uniform;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inDiffuseColor;
layout(location = 2) in vec3 inNormal;

out vec4 diffuseColor;
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

	diffuseColor = inDiffuseColor;
	vertexNormal = normalModelToCameraMatrix * inNormal;
	cameraSpacePosition = vec3(cameraPosition);
}
