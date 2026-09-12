#version 330

// For meshes that carry per-vertex colours: the "lit-color" VAO, plus Ground,
// whose default VAO supplies all three attributes.
//
// The material's diffuse colour is constant across a primitive, so folding it
// in here and interpolating gives exactly the same result as interpolating
// first and multiplying per fragment -- and it lets one fragment shader serve
// both vertex-coloured and material-coloured objects.

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

// Instance-named so its members do not collide with the `diffuseColor` varying
// above. Without `Mtl` they would land in global scope and clash.
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

	diffuseColor = inDiffuseColor * Mtl.diffuseColor;
	vertexNormal = normalModelToCameraMatrix * inNormal;
	cameraSpacePosition = vec3(cameraPosition);
}
