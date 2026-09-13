#version 330

// The ray-traced impostor, with the depth buffer let in on the secret.
//
// The rasterizer interpolates depth from the square's vertices, so until now
// every impostor fragment has been tested and recorded at the depth of the
// square -- a plane through the sphere's centre. Anything between that plane
// and the sphere's near surface wrongly loses to it; anything between the
// plane and the far surface wrongly wins. Watch a sphere orbit through the
// ground plane with this shader off: the intersection is a straight cut where
// a real sphere would show a circle.
//
// The fix is to overwrite the depth with the one the ray-traced surface would
// have had -- see frag_depth.glsl.
//
// It is not free. A fragment shader that writes gl_FragDepth *anywhere*
// forfeits early depth testing for the whole program: the hardware can no
// longer reject a hidden fragment before running the shader, because the
// shader is what decides the depth. Every fragment of every impostor square
// now runs the lighting model, hidden or not. And the write must happen on
// every path that does not discard -- a fragment that reaches the end without
// one has undefined depth, not the rasterizer's.

#include "material.glsl"
#include "ray_sphere.glsl"
#include "frag_depth.glsl"

in vec2 mapping;

out vec4 outputColor;

uniform float sphereRadius;
uniform vec3 cameraSpherePos;

void main() {
	vec3 cameraPos;
	vec3 cameraNormal;
	impostor(mapping, cameraSpherePos, sphereRadius, cameraPos, cameraNormal);

	gl_FragDepth = windowDepth(cameraPos);

	outputColor = gammaCorrect(accumulateLighting(Mtl.material, cameraPos, cameraNormal));
}
