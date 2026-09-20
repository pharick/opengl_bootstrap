#version 330

// One shininess for the whole surface, looked up in the 2D table.
//
// This is Basic Texture's texture path, moved to a 2D texture: the material's
// shininess selects a row, the cosine selects a column. It exists so the
// other two modes have something to be compared against -- and so you can see
// that the second axis costs nothing. Moving the shininess slider in Basic
// Texture rebuilt every table; here it just moves T.

#include "lighting.glsl"

in vec3 vertexNormal;
in vec3 cameraSpacePosition;

out vec4 outputColor;

// The table is 2D now, so the sampler is too. Same rule as before: the
// sampler type has to match the texture's target, or the fetch is undefined.
uniform sampler2D gaussianTexture;

float specularTerm(in float cosAngNormalHalf, in float specularShininess) {
	// S and T are the two arguments of the function, in the order the table
	// was built: cosine along the row, shininess down the rows. Both are
	// already on [0, 1], so no remapping is needed to make them coordinates.
	vec2 texCoord = vec2(cosAngNormalHalf, specularShininess);
	return texture(gaussianTexture, texCoord).r;
}

void main() {
	vec3 surfaceNormal = normalize(vertexNormal);

	outputColor =
	    gammaCorrect(accumulateLighting(cameraSpacePosition, surfaceNormal, Mtl.specularShininess));
}
