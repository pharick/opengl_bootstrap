// The ray-traced sphere impostor, shared by every fragment shader that does it
// (impostor_persp, impostor_depth, impostor_geom). They differ in where the
// inputs come from -- uniforms for one sphere per draw, a geometry shader's
// outputs for a batch -- and in what they do with the result, so the function
// takes the sphere as arguments and leaves both to the caller.

#ifndef TUT13_RAY_SPHERE_GLSL
#define TUT13_RAY_SPHERE_GLSL

/// Ray-sphere intersection in camera space, where the eye is at the origin.
///
/// `mapping` is the fragment's position on the impostor square, in sphere
/// radii from the centre. The ray is P = D*t with D unit length, so a point on
/// it is inside the sphere when |D*t - S| = R. Expanding the dot product gives
/// a quadratic in t:
///
///     (D.D) t^2  +  2 (D . -S) t  +  (S.S - R^2)  =  0
///        A              B                C
///
/// and A is 1 because D is normalized. No real root means the ray misses;
/// otherwise the smaller root is where it enters, which is the surface we see.
/// The position is then just D*t, and the normal is the direction out from the
/// centre to that point -- which is what a normal is on a sphere.
void impostor(in vec2 mapping, in vec3 cameraSpherePos, in float sphereRadius, out vec3 cameraPos,
              out vec3 cameraNormal) {
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
