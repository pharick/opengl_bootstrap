#version 330

layout(std140) uniform;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inDiffuseColor;
layout(location = 2) in vec3 inNormal;

out vec4 diffuseColor;
out vec3 vertexNormal;
out vec3 cameraSpacePosition;

uniform Projection
{
    mat4 cameraToClipMatrix;
};

uniform mat4 modelToCameraMatrix;
uniform mat3 normalModelToCameraMatrix;

void main() {
    vec4 cameraPosition = modelToCameraMatrix * vec4(inPosition, 1.0);
    gl_Position = cameraToClipMatrix * cameraPosition;

    diffuseColor = inDiffuseColor;
    vertexNormal = normalize(normalModelToCameraMatrix * inNormal);
    cameraSpacePosition = vec3(cameraPosition);
}
