#version 330

layout(std140) uniform;

in vec4 diffuseColor;
in vec3 vertexNormal;
in vec3 cameraSpacePosition;

out vec4 outputColor;

struct PerLight
{
    vec4 cameraSpaceLightPos;
    vec4 lightIntensity;
};

const int numberOfLights = 4;

uniform Light
{
    vec4 ambientIntensity;
    float lightAttenuation;
    PerLight lights[numberOfLights];
} Lgt;

float calcAttenuation(vec3 cameraSpacePosition, vec3 cameraSpaceLightPos, out vec3 lightDirection) {
    vec3 lightDifference = cameraSpaceLightPos - cameraSpacePosition;
    float lightDistanceSqrt = dot(lightDifference, lightDifference);
    lightDirection = lightDifference * inversesqrt(lightDistanceSqrt);
    return 1.0 / (1.0 + Lgt.lightAttenuation * lightDistanceSqrt);
}

vec4 computeLighting(PerLight light) {
    vec3 lightDirection;
    vec4 lightIntensity;

    if (light.cameraSpaceLightPos.w == 0.0) {
        // Directional light
        lightDirection = normalize(light.cameraSpaceLightPos.xyz);
        lightIntensity = light.lightIntensity;
    } else {
        // Point light
        float attenuation = calcAttenuation(cameraSpacePosition, light.cameraSpaceLightPos.xyz, lightDirection);
        lightIntensity = attenuation * light.lightIntensity;
    }

    vec3 surfaceNormal = normalize(vertexNormal);

    float diffuseFactor = dot(surfaceNormal, lightDirection);
    diffuseFactor = diffuseFactor < 0.0001 ? 0.0 : diffuseFactor;

    return diffuseColor * lightIntensity * diffuseFactor;
}

void main() {
    vec4 finalLighting = diffuseColor * Lgt.ambientIntensity;

    for (int i = 0; i < numberOfLights; ++i) {
        finalLighting += computeLighting(Lgt.lights[i]);
    }

    outputColor = finalLighting;
}
