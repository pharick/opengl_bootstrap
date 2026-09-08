#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

uniform mat4 modelToCamera;

uniform vec3 dirToLight;
uniform vec3 lightIntensity;
uniform vec3 ambientIntensity;
uniform mat3 normalModelToCamera;

layout(std140) uniform Projection {
    mat4 cameraToClip;
};

out vec3 vertexColor;

void main() {
    gl_Position = cameraToClip * (modelToCamera * vec4(inPosition, 1.0));

    vec3 normalCamera = normalize(normalModelToCamera * inNormal);
    float cosAngleIncidence = dot(normalCamera, dirToLight);
    vertexColor = inColor * lightIntensity * clamp(cosAngleIncidence, 0.0, 1.0) + inColor * ambientIntensity;
}
