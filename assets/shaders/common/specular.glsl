// The three specular models from Tutorial 11 (Shinies).
//
//     #include <common/specular.glsl>
//
// Each returns the bare term, without the "is this surface even facing the
// light" guard -- that check belongs to the caller, who has the angle of
// incidence already and should apply it once regardless of which model is
// selected. None of these read uniforms, so a shader can name its own however
// it likes.
//
// All three take directions pointing *away* from the surface: dirToLight
// toward the lamp, dirToViewer toward the camera. In camera space the latter is
// just normalize(-cameraSpacePosition), since the eye sits at the origin.
//
// The exponent and the Gaussian width are not the same quantity and do not
// share a range. Larger exponents mean tighter highlights; larger widths mean
// broader ones.

#ifndef GLC_SPECULAR_GLSL
#define GLC_SPECULAR_GLSL

/// Phong: how closely the mirror bounce of the light points at the eye.
///
/// Cheap and famous, but it has a real failure mode. Once the reflection and
/// the view direction are more than 90 degrees apart the dot product goes
/// negative and every fragment beyond that boundary clamps to exactly zero --
/// so the falloff does not ease out, it hits a wall. Visible as a hard-edged
/// highlight at low exponents and grazing angles.
float phongTerm(in vec3 normal, in vec3 dirToLight, in vec3 dirToViewer, in float exponent) {
	// reflect() wants the incident direction, which points *at* the surface.
	vec3 reflection = reflect(-dirToLight, normal);
	return pow(clamp(dot(dirToViewer, reflection), 0.0, 1.0), exponent);
}

/// Blinn-Phong: how closely the surface normal matches the orientation that
/// *would* bounce the light straight at the eye.
///
/// The half-angle vector is the bisector of dirToLight and dirToViewer, so on
/// any lit, visible fragment
///
///     dot(N, H) = (dot(N, L) + dot(N, V)) / length(L + V)
///
/// and both terms of that numerator are positive. It can never reach the clamp,
/// which is exactly the discontinuity Phong suffers from. The price is that the
/// exponents are not interchangeable: Blinn needs roughly 4x Phong's for a
/// similarly sized highlight, because the angle it measures is about half as
/// large.
float blinnTerm(in vec3 normal, in vec3 dirToLight, in vec3 dirToViewer, in float exponent) {
	vec3 halfAngle = normalize(dirToLight + dirToViewer);
	return pow(clamp(dot(normal, halfAngle), 0.0, 1.0), exponent);
}

/// Gaussian: the same half-angle as Blinn, but a bell curve over the *angle*
/// rather than a power of its cosine.
///
/// Closer in spirit to how real microfacet distributions are modelled (this is
/// essentially Beckmann). `width` is an angular spread in radians, so useful
/// values are small -- roughly 0.02 to 1.0 -- and bigger means rougher.
float gaussianTerm(in vec3 normal, in vec3 dirToLight, in vec3 dirToViewer, in float width) {
	vec3 halfAngle = normalize(dirToLight + dirToViewer);

	// The clamp is load-bearing, not defensive tidiness. The dot product of two
	// unit vectors is mathematically in [-1, 1], but floating point rounding
	// hands back 1.0000001 often enough to matter, and acos() of anything above
	// 1 is NaN. That NaN survives exp(), survives the multiply, and lands in the
	// output colour as a bad pixel -- right at the centre of the highlight,
	// appearing and vanishing as the light moves. gltut's own shader omits this.
	float cosAngle = clamp(dot(halfAngle, normal), -1.0, 1.0);

	float scaled = acos(cosAngle) / width;
	return exp(-(scaled * scaled));
}

#endif
