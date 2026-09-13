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
// it enters the sphere. That is a ray tracer, one sphere wide.

#include "lighting.glsl"

in vec2 mapping;

out vec4 outputColor;

uniform float sphereRadius;
uniform vec3 cameraSpherePos;

/// Ray-sphere intersection in camera space, where the eye is at the origin.
///
/// The ray is P = D*t with D unit length, so a point on it is inside the sphere
/// when |D*t - S| = R. Expanding the dot product gives a quadratic in t:
///
///     (D.D) t^2  +  2 (D . -S) t  +  (S.S - R^2)  =  0
///        A              B                C
///
/// and A is 1 because D is normalized. No real root means the ray misses;
/// otherwise the smaller root is where it enters, which is the surface we see.
/// The position is then just D*t, and the normal is the direction out from the
/// centre to that point -- which is what a normal is on a sphere.
void impostor(out vec3 cameraPos, out vec3 cameraNormal) {
	// The fragment's point on the square, in camera space. The square is
	// boxCorrection radii wide, and so is mapping, so this is right regardless.
	vec3 cameraPlanePos = vec3(mapping * sphereRadius, 0.0) + cameraSpherePos;
	vec3 rayDirection = normalize(cameraPlanePos);

	float B = 2.0 * dot(rayDirection, -cameraSpherePos);
	float C = dot(cameraSpherePos, cameraSpherePos) - (sphereRadius * sphereRadius);

	float det = (B * B) - (4.0 * C);
	if (det < 0.0) {
		discard;
	}

	float sqrtDet = sqrt(det);
	float posT = (-B + sqrtDet) / 2.0;
	float negT = (-B - sqrtDet) / 2.0;

	float intersectT = min(posT, negT);
	cameraPos = rayDirection * intersectT;
	cameraNormal = normalize(cameraPos - cameraSpherePos);
}

void main() {
	vec3 cameraPos;
	vec3 cameraNormal;
	impostor(cameraPos, cameraNormal);

	outputColor = gammaCorrect(accumulateLighting(cameraPos, cameraNormal));
}
