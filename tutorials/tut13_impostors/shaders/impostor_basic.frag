#version 330

// A sphere that is not there.
//
// The rasterizer hands over fragments of a flat square. For each one this
// shader works out where a sphere of the given radius would have been hit by a
// ray through that fragment -- or that it would not have been, in which case
// the fragment is discarded and the square's corner never reaches the screen.
// What is left is a disc of fragments carrying the position and normal a real
// sphere would have had, and the lighting model cannot tell the difference.
//
// Two lies remain. The "ray" here is assumed to travel straight down -Z for
// every fragment, which is only true at the centre of the screen -- see
// impostor_persp.frag for the fix. And the depth values are the square's, not
// the sphere's, which is what the chapter fixes after that.

#include "lighting.glsl"

in vec2 mapping;

out vec4 outputColor;

uniform float sphereRadius;
uniform vec3 cameraSpherePos;

/// `mapping` is the fragment's position on the square, in [-1, 1] on each axis,
/// which makes it a point on the unit circle's bounding box. Inside the circle,
/// Pythagoras gives the z of the unit sphere above it -- and a point on the
/// unit sphere is its own normal. Scale by the radius and offset to the centre
/// and that is the camera-space position too.
void impostor(out vec3 cameraPos, out vec3 cameraNormal) {
	float lengthSquared = dot(mapping, mapping);
	if (lengthSquared > 1.0) {
		discard;
	}

	cameraNormal = vec3(mapping, sqrt(1.0 - lengthSquared));
	cameraPos = (cameraNormal * sphereRadius) + cameraSpherePos;
}

void main() {
	vec3 cameraPos;
	vec3 cameraNormal;
	impostor(cameraPos, cameraNormal);

	outputColor = gammaCorrect(accumulateLighting(cameraPos, cameraNormal));
}
