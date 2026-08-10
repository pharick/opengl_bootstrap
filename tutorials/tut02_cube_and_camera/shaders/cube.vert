#version 330

// Locations match the <attribute index="..."> entries in assets/meshes/UnitCube.xml.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

uniform mat4 modelToClip;

out vec3 vertexColor;

void main() {
	gl_Position = modelToClip * vec4(inPosition, 1.0);
	vertexColor = inColor;
}
