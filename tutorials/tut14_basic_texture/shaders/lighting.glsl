// The lighting model for this chapter, shared by the two fragment shaders that
// differ only in where the specular term comes from.
//
// The book ships ShaderGaussian.frag and TextureGaussian.frag as near-identical
// copies of one file, and the only lines that differ are the three that produce
// the Gaussian. Here that difference is a single function, declared below and
// defined by whichever shader includes this -- so what the Spacebar switches
// between really is those definitions and nothing else.
//
// Its one argument is the whole point of the chapter. Gaussian specular takes a
// surface normal, a half-angle vector and a shininess; the shininess is
// constant over a mesh, and the two vectors only ever appear as their dot
// product. So for a given material it is a function of one variable on [0, 1]
// -- and a function of one variable is something you can tabulate.

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
	/// Read by shader_gaussian.frag only: the texture path has this value
	/// baked into the table and cannot be told about a new one.
	float specularShininess;
}
Mtl;

/// Defined by the including shader. `cosAngNormalHalf` is the cosine of the
/// angle between the surface normal and the half-angle vector, on [0, 1]. The
/// return value is the bare specular term, with no "is this surface even facing
/// the light" guard -- that is applied once, below, so both definitions get it.
float specularTerm(in float cosAngNormalHalf);

float calcAttenuation(in vec3 cameraSpacePosition, in vec3 cameraSpaceLightPos,
                      out vec3 lightDirection) {
	vec3 lightDifference = cameraSpaceLightPos - cameraSpacePosition;
	float lightDistanceSquared = dot(lightDifference, lightDifference);
	lightDirection = lightDifference * inversesqrt(lightDistanceSquared);
	return 1.0 / (1.0 + Lgt.lightAttenuation * lightDistanceSquared);
}

vec4 computeLighting(in PerLight light, in vec3 cameraSpacePosition, in vec3 surfaceNormal) {
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

	// The one variable the specular function is left with.
	//
	// Clamping is what makes the two definitions comparable. The table only
	// covers [0, 1] and its sampler clamps to the edge, so the texture path
	// treats everything below 0 as 0 whether we ask it to or not; doing the
	// same here means the shader path is not being compared against a
	// differently-shaped function. It also keeps acos() away from 1.0000001,
	// whose NaN would otherwise survive to the framebuffer as a bad pixel at
	// the dead centre of the highlight.
	float cosAngNormalHalf = clamp(dot(halfAngle, surfaceNormal), 0.0, 1.0);

	// No highlight on a surface that faces away from the lamp.
	float specular = cosAngIncidence > 0.0 ? specularTerm(cosAngNormalHalf) : 0.0;

	return (Mtl.diffuseColor * lightIntensity * cosAngIncidence) +
	       (Mtl.specularColor * lightIntensity * specular);
}

/// Ambient plus every light in the block. `surfaceNormal` must be unit length.
vec4 accumulateLighting(in vec3 cameraSpacePosition, in vec3 surfaceNormal) {
	vec4 accumLighting = Mtl.diffuseColor * Lgt.ambientIntensity;
	for (int light = 0; light < numberOfLights; ++light) {
		accumLighting += computeLighting(Lgt.lights[light], cameraSpacePosition, surfaceNormal);
	}
	return accumLighting;
}

/// The book's shortcut: sqrt is pow(x, 1/2), a gamma of 2.0 rather than 2.2.
vec4 gammaCorrect(in vec4 linear) {
	return sqrt(linear);
}

#endif
