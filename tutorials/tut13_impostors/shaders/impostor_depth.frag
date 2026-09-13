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
// have had, by running the position through the same pipeline the rasterizer
// would: camera space -> clip space (projection matrix) -> NDC (divide by w)
// -> window space (the glDepthRange mapping, which GLSL exposes as
// gl_DepthRange).
//
// It is not free. A fragment shader that writes gl_FragDepth *anywhere*
// forfeits early depth testing for the whole program: the hardware can no
// longer reject a hidden fragment before running the shader, because the
// shader is what decides the depth. Every fragment of every impostor square
// now runs the lighting model, hidden or not. And the write must happen on
// every path that does not discard -- a fragment that reaches the end without
// one has undefined depth, not the rasterizer's.

#include "lighting.glsl"
#include "ray_sphere.glsl"

out vec4 outputColor;

uniform Projection {
	mat4 cameraToClipMatrix;
};

/// What gl_FragCoord.z would have been for a fragment at `cameraPos`.
float windowDepth(in vec3 cameraPos) {
	vec4 clipPos = cameraToClipMatrix * vec4(cameraPos, 1.0);
	float ndcDepth = clipPos.z / clipPos.w;

	// glDepthRange(near, far) maps NDC [-1, 1] onto [near, far]. Written the
	// way the spec states it, with diff = far - near.
	return ((gl_DepthRange.diff * ndcDepth) + gl_DepthRange.near + gl_DepthRange.far) / 2.0;
}

void main() {
	vec3 cameraPos;
	vec3 cameraNormal;
	impostor(cameraPos, cameraNormal);

	gl_FragDepth = windowDepth(cameraPos);

	outputColor = gammaCorrect(accumulateLighting(cameraPos, cameraNormal));
}
