#version 330

// Per-fragment diffuse + specular for a single attenuated point light.
//
// The vertex shader hands over an interpolated camera-space position and normal
// and nothing else; every lighting decision is made here, per pixel. Working in
// camera space makes the view direction free -- the eye is at the origin, so the
// direction to it is just normalize(-cameraSpacePosition).

#include <common/lighting.glsl>
#include <common/specular.glsl>

// Must match the SpecularModel and LightingMode enums in main.cpp.
const int kModelPhong = 0;
const int kModelBlinnPhong = 1;
const int kModelGaussian = 2;

const int kLightingAll = 0;
const int kLightingDiffuseOnly = 1;
const int kLightingSpecularOnly = 2;

uniform vec3 lightPosition; // camera space
uniform vec3 lightIntensity;
uniform vec3 ambientIntensity;
uniform float lightAttenuation;
uniform bool useInverseSquaredAttenuation;

uniform vec3 diffuseColor;
uniform vec3 specularColor;
uniform float shininessFactor; // Phong / Blinn exponent
uniform float gaussianWidth;   // Gaussian angular spread, radians

uniform int specularModel;
uniform int lightingMode;

in vec3 cameraSpacePosition;
in vec3 cameraSpaceNormal;

out vec4 outColor;

void main() {
	// Interpolating two unit vectors does not produce a unit vector, so the
	// normal has to be renormalized here rather than in the vertex shader.
	vec3 normal = normalize(cameraSpaceNormal);
	vec3 dirToViewer = normalize(-cameraSpacePosition);

	vec3 dirToLight;
	vec3 intensity =
	    applyLightIntensity(cameraSpacePosition, lightPosition, lightIntensity, lightAttenuation,
		                    useInverseSquaredAttenuation, dirToLight);

	float cosAngIncidence = clamp(dot(normal, dirToLight), 0.0, 1.0);

	float specularTerm;
	if (specularModel == kModelPhong) {
		specularTerm = phongTerm(normal, dirToLight, dirToViewer, shininessFactor);
	} else if (specularModel == kModelBlinnPhong) {
		specularTerm = blinnTerm(normal, dirToLight, dirToViewer, shininessFactor);
	} else {
		specularTerm = gaussianTerm(normal, dirToLight, dirToViewer, gaussianWidth);
	}

	// One guard for all three models. Without it a surface turned away from the
	// lamp can still line its reflection up with the eye and produce a bright
	// highlight where the diffuse term is exactly zero -- a lit patch on the
	// dark side, shining through the object.
	specularTerm = cosAngIncidence > 0.0 ? specularTerm : 0.0;

	vec3 diffuse = diffuseColor * intensity * cosAngIncidence;
	vec3 specular = specularColor * intensity * specularTerm;

	// Ambient survives in every mode, so an isolated term still has a silhouette
	// to sit on rather than floating in the void.
	vec3 color = diffuseColor * ambientIntensity;
	if (lightingMode != kLightingSpecularOnly) {
		color += diffuse;
	}
	if (lightingMode != kLightingDiffuseOnly) {
		color += specular;
	}

	outColor = vec4(color, 1.0);
}
