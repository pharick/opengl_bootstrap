#ifndef GLC_LIGHTING_GLSL
#define GLC_LIGHTING_GLSL

vec3 ApplyLightIntensity(in vec3 cameraSpacePosition, in vec3 lightPosition, in vec3 lightIntensity,
                         in float lightAttenuation, in bool useInverseSquaredAttenuation,
                         out vec3 lightDirection) {
	vec3 lightDifference = lightPosition - cameraSpacePosition;
	float lightDistanceSquared = dot(lightDifference, lightDifference);
	lightDirection = lightDifference * inversesqrt(lightDistanceSquared);
	float distanceFactor =
	    useInverseSquaredAttenuation ? lightDistanceSquared : sqrt(lightDistanceSquared);
	return lightIntensity / (1.0 + lightAttenuation * distanceFactor);
}

#endif
