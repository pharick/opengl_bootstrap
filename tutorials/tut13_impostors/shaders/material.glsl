// One material per draw, bound as a slice of the material buffer. Every shader
// that draws one object at a time includes this; impostor_geom.frag, which
// draws all four spheres in one call, declares an array instead.
//
// A block whose only member is a struct lays out exactly like a block with the
// struct's members inline, so the C++ side uploads the same 48 bytes either
// way.

#ifndef TUT13_MATERIAL_GLSL
#define TUT13_MATERIAL_GLSL

#include "lighting.glsl"

uniform Material {
	MaterialEntry material;
}
Mtl;

#endif
