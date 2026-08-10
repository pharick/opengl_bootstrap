#version 330

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

uniform mat4 modelToClip;

out vec2 texCoord;

void main() {
	gl_Position = modelToClip * vec4(inPosition, 0.0, 1.0);
	texCoord = inTexCoord;
}
