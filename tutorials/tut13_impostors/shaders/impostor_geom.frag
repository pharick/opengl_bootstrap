#version 330

// The depth-correct ray-traced impostor, for a whole batch of spheres in one
// draw call.
//
// The shading is impostor_depth.frag's exactly. What changes is plumbing: the
// sphere arrives from the geometry shader's interface block rather than from
// uniforms, and the material is chosen per primitive out of an array, indexed
// by the sphere's index -- which the geometry shader took from the input
// point's index, which is the sphere's index in the vertex buffer, which is
// its index in the material array. (The book uses gl_PrimitiveID for this;
// see impostor_geom.geom for why this does not.) All four spheres, one
// glDrawArrays, no per-object uniform or buffer binding at all.
//
// Uniform blocks have a hardware size limit (GL_MAX_UNIFORM_BLOCK_SIZE, at
// least 16 KB), so this scales to hundreds of materials, not millions. Past
// that the chapter's answer is "several draw calls"; later ones have better.

#include "lighting.glsl"
#include "ray_sphere.glsl"
#include "frag_depth.glsl"

// Must match the geometry shader's output block member for member. Unnamed,
// so the members are plain globals here.
in FragData {
	flat vec3 cameraSpherePos;
	flat float sphereRadius;
	flat int sphereIndex;
	smooth vec2 mapping;
};

out vec4 outputColor;

const int numberOfSpheres = 4;

uniform Material {
	MaterialEntry material[numberOfSpheres];
}
Mtl;

void main() {
	vec3 cameraPos;
	vec3 cameraNormal;
	impostor(mapping, cameraSpherePos, sphereRadius, cameraPos, cameraNormal);

	gl_FragDepth = windowDepth(cameraPos);

	outputColor =
	    gammaCorrect(accumulateLighting(Mtl.material[sphereIndex], cameraPos, cameraNormal));
}
