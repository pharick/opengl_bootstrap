#version 330

// A sphere that is not there -- this time seen from where the camera actually
// is.
//
// impostor_basic.frag assumed every fragment looks straight down -Z, so the
// visible hemisphere is always the +Z one and the outline is always a circle.
// Under perspective neither is true: a sphere off to the side is seen along a
// slanted ray, so its outline is an ellipse and the part facing the camera is
// tilted toward it. The basic impostor gets both wrong, and the further from
// the screen centre, the worse.
//
// Fixing it means doing what the rasterizer does not: for each fragment, fire
// a ray from the eye through the fragment's point on the square and find where
// it enters the sphere. That is a ray tracer, one sphere wide. See
// ray_sphere.glsl for the maths.
//
// The one lie left is depth. Every fragment here still carries the depth of
// the flat square, so the depth test compares the *square* against the scene,
// and a sphere half-buried in the ground plane is cut off along a straight
// line. impostor_depth.frag fixes that.

#include "lighting.glsl"
#include "ray_sphere.glsl"

out vec4 outputColor;

void main() {
	vec3 cameraPos;
	vec3 cameraNormal;
	impostor(cameraPos, cameraNormal);

	outputColor = gammaCorrect(accumulateLighting(cameraPos, cameraNormal));
}
