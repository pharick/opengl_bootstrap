#version 330

// For meshes drawn through the "lit" VAO, which supplies position and normal
// only. Attribute 1 is not bound at all here, so the diffuse colour comes
// entirely from the material block.
//
// This is the whole reason two vertex shaders exist. Reading inDiffuseColor
// from a VAO that does not provide it yields the disabled-attribute constant
// (0, 0, 0, 1), which multiplies every material to black -- silently.

layout(std140) uniform;

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec3 inNormal;

out vec4 diffuseColor;
out vec3 vertexNormal;
out vec3 cameraSpacePosition;

uniform Projection {
	mat4 cameraToClipMatrix;
};

uniform Material {
	vec4 diffuseColor;
	vec4 specularColor;
	float specularShininess;
}
Mtl;

uniform mat4 modelToCameraMatrix;
uniform mat3 normalModelToCameraMatrix;

void main() {
	vec4 cameraPosition = modelToCameraMatrix * vec4(inPosition, 1.0);
	gl_Position = cameraToClipMatrix * cameraPosition;

	diffuseColor = Mtl.diffuseColor;
	vertexNormal = normalModelToCameraMatrix * inNormal;
	cameraSpacePosition = vec3(cameraPosition);
}
