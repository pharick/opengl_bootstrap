#version 330

// One vertex per sphere, straight through.
//
// The other impostors get their sphere from uniforms and draw one square per
// glDrawArrays. Here the spheres are vertex data -- position and radius,
// interleaved in one buffer -- so a single draw of GL_POINTS covers all of
// them, and the geometry shader turns each point into a square.
//
// There is no gl_Position. A vertex shader only has to produce one when it is
// the last stage before rasterization; with a geometry shader after it, the
// geometry shader is, and it writes gl_Position for every vertex it emits.
//
// The outputs are grouped into an interface block. For a lone vertex shader
// that is just organization; for the geometry shader consuming it, it is what
// lets the whole set be declared once as an array of per-vertex inputs.

layout(location = 0) in vec3 inCameraSpherePos;
layout(location = 1) in float inSphereRadius;

out VertexData {
	vec3 cameraSpherePos;
	float sphereRadius;
}
outData;

void main() {
	outData.cameraSpherePos = inCameraSpherePos;
	outData.sphereRadius = inSphereRadius;
}
