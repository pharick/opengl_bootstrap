// Shared GLSL, reachable from any tutorial as:
//
//     #include <gamma.glsl>          // resolved against assets/shaders
//     #include "common/gamma.glsl"   // also works
//
// glcore expands these before handing the source to the driver and emits #line
// directives, so compiler errors still point at the right file and line.

#ifndef GLC_GAMMA_GLSL
#define GLC_GAMMA_GLSL

// Approximation used throughout Tutorial 12 (Dynamic Range) and 16 (Gamma and
// Textures). Prefer an sRGB texture format or an sRGB framebuffer where you
// can; reach for these when you need the conversion explicitly.

vec3 linearToSrgb(vec3 linear) {
	return pow(linear, vec3(1.0 / 2.2));
}

vec3 srgbToLinear(vec3 srgb) {
	return pow(srgb, vec3(2.2));
}

#endif
