// The lighting model for this example, shared by the three fragment shaders.
//
// In Basic Texture, specularTerm() took one argument. The shininess was a
// material constant, so the Gaussian was a function of the one thing left
// over -- the cosine between normal and half-angle vector -- and the table
// could bake the shininess in. Here the shininess is allowed to vary from
// fragment to fragment, so it comes back as a second argument, the Gaussian is
// a function of two variables again, and a function of two variables is a 2D
// texture.
//
// The three shaders that include this differ on exactly two points, and the
// book shows three of the four combinations:
//
//                        shininess from Mtl     shininess from a texture
//   Gaussian from table  fixed_shininess.frag   texture_shininess.frag
//   Gaussian computed    (Tutorial 11)          texture_compute.frag
//
// How the Gaussian is evaluated is specularTerm(), defined by the including
// shader. Where the shininess comes from is decided in that shader's main()
// and passed down, which is the book's structure too.

#ifndef TUT14_LIGHTING_GLSL
#define TUT14_LIGHTING_GLSL

layout(std140) uniform;

struct PerLight {
	vec4 cameraSpaceLightPos;
	vec4 lightIntensity;
};

const int numberOfLights = 2;

uniform Light {
	vec4 ambientIntensity;
	float lightAttenuation;
	PerLight lights[numberOfLights];
}
Lgt;

uniform Material {
	vec4 diffuseColor;
	vec4 specularColor;
	/// Read by fixed_shininess.frag only. The other two get their shininess
	/// from a texture and never look at this.
	float specularShininess;
}
Mtl;

/// Defined by the including shader. Both arguments are on [0, 1]:
/// `cosAngNormalHalf` is the cosine of the angle between the surface normal
/// and the half-angle vector, `specularShininess` the Gaussian's width in
/// radians. The return value is the bare specular term; the "is this surface
/// even facing the light" guard is applied once, below, for every definition.
float specularTerm(in float cosAngNormalHalf, in float specularShininess);

float calcAttenuation(in vec3 cameraSpacePosition, in vec3 cameraSpaceLightPos,
                      out vec3 lightDirection) {
	vec3 lightDifference = cameraSpaceLightPos - cameraSpacePosition;
	float lightDistanceSquared = dot(lightDifference, lightDifference);
	lightDirection = lightDifference * inversesqrt(lightDistanceSquared);
	return 1.0 / (1.0 + Lgt.lightAttenuation * lightDistanceSquared);
}

vec4 computeLighting(in PerLight light, in vec3 cameraSpacePosition, in vec3 surfaceNormal,
                     in float specularShininess) {
	vec3 lightDirection;
	vec4 lightIntensity;

	// w discriminates the two kinds: a direction gets no attenuation.
	if (light.cameraSpaceLightPos.w == 0.0) {
		lightDirection = normalize(light.cameraSpaceLightPos.xyz);
		lightIntensity = light.lightIntensity;
	} else {
		float attenuation =
		    calcAttenuation(cameraSpacePosition, light.cameraSpaceLightPos.xyz, lightDirection);
		lightIntensity = attenuation * light.lightIntensity;
	}

	float cosAngIncidence = clamp(dot(surfaceNormal, lightDirection), 0.0, 1.0);

	// Camera space puts the eye at the origin, so the direction to it is free.
	vec3 dirToViewer = normalize(-cameraSpacePosition);
	vec3 halfAngle = normalize(lightDirection + dirToViewer);

	// Clamped so the computed and the tabulated definitions are the same
	// function: the table covers [0, 1] and its sampler clamps to the edge,
	// so the texture path treats everything below 0 as 0 whether asked to or
	// not. It also keeps acos() away from 1.0000001, whose NaN would land in
	// the framebuffer as a bad pixel at the dead center of the highlight.
	float cosAngNormalHalf = clamp(dot(halfAngle, surfaceNormal), 0.0, 1.0);

	// No highlight on a surface that faces away from the lamp.
	float specular =
	    cosAngIncidence > 0.0 ? specularTerm(cosAngNormalHalf, specularShininess) : 0.0;

	return (Mtl.diffuseColor * lightIntensity * cosAngIncidence) +
	       (Mtl.specularColor * lightIntensity * specular);
}

/// Ambient plus every light in the block. `surfaceNormal` must be unit length.
vec4 accumulateLighting(in vec3 cameraSpacePosition, in vec3 surfaceNormal,
                        in float specularShininess) {
	vec4 accumLighting = Mtl.diffuseColor * Lgt.ambientIntensity;
	for (int light = 0; light < numberOfLights; ++light) {
		accumLighting += computeLighting(Lgt.lights[light], cameraSpacePosition, surfaceNormal,
		                                 specularShininess);
	}
	return accumLighting;
}

/// The book's shortcut: sqrt is pow(x, 1/2), a gamma of 2.0 rather than 2.2.
vec4 gammaCorrect(in vec4 linear) {
	return sqrt(linear);
}

#endif
