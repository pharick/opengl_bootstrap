#version 330

// Lights a surface whose position and normal were interpolated from vertices.
// Compare impostor.frag, which feeds the same model a surface it made up.

#include "lighting.glsl"

in vec3 vertexNormal;
in vec3 cameraSpacePosition;

out vec4 outputColor;

void main() {
	// Interpolating two unit vectors does not produce a unit vector.
	vec3 surfaceNormal = normalize(vertexNormal);

	outputColor = gammaCorrect(accumulateLighting(cameraSpacePosition, surfaceNormal));
}
