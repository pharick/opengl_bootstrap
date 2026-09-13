#version 330

// Lights a surface whose position and normal were interpolated from vertices.
// Compare the impostor_*.frag shaders, which feed the same model a surface
// they made up.

#include "material.glsl"

in vec3 vertexNormal;
in vec3 cameraSpacePosition;

out vec4 outputColor;

void main() {
	// Interpolating two unit vectors does not produce a unit vector.
	vec3 surfaceNormal = normalize(vertexNormal);

	outputColor = gammaCorrect(accumulateLighting(Mtl.material, cameraSpacePosition, surfaceNormal));
}
