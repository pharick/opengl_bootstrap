// The ray-traced sphere impostor, shared by impostor_persp.frag and
// impostor_depth.frag. The two differ only in what they do with the result:
// one lights it, the other lights it and also tells the depth buffer about it.
//
// Declares the inputs it reads, so an including shader commits to the same
// `mapping` varying and uniforms that impostor.vert produces.

#ifndef TUT13_RAY_SPHERE_GLSL
#define TUT13_RAY_SPHERE_GLSL

in vec2 mapping;

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

#endif
