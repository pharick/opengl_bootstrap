#version 330

// A point in, a camera-facing square out.
//
// A geometry shader sits between the vertex shader and the rasterizer and
// works on whole primitives rather than vertices: it is handed every vertex of
// one input primitive at once, and it emits zero or more output primitives,
// vertex by vertex. It is the one stage that can change *how many* things get
// drawn -- and, as here, what shape they are.
//
// Both layout lines are mandatory. The input one has to match the draw call's
// primitive type (GL_POINTS, so every "primitive" is one vertex, and the input
// arrays have length 1). The output one names the primitive type produced and
// an upper bound on vertices per invocation, which the driver uses to size the
// output buffer; emitting more than max_vertices is undefined.
//
// Clipping and face culling happen *after* this stage, so the corner order
// matters here exactly as it did in impostor.vert: bottom-left, top-left,
// bottom-right, top-right winds the strip's first triangle clockwise, which is
// this program's front face.

layout(std140) uniform;
layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

uniform Projection {
	mat4 cameraToClipMatrix;
};

uniform float boxCorrection;

// Every input is an array, one entry per vertex of the input primitive -- even
// for points, where there is only vert[0].
in VertexData {
	vec3 cameraSpherePos;
	float sphereRadius;
}
vert[];

// `flat`: not interpolated across the primitive. These are per-sphere values
// that happen to be delivered per-vertex, so every corner carries the same
// ones and interpolating would only add rounding. `mapping` is the one thing
// that genuinely varies across the square.
out FragData {
	flat vec3 cameraSpherePos;
	flat float sphereRadius;
	flat int sphereIndex;
	smooth vec2 mapping;
};

const vec2 corners[4] = vec2[4](vec2(-1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(1.0, 1.0));

void main() {
	// A sphere the CPU wants drawn as a mesh this frame is sent with radius 0.
	// Emitting nothing for it is the cheapest possible cull: the primitive
	// simply never reaches the rasterizer.
	if (vert[0].sphereRadius <= 0.0) {
		return;
	}

	for (int i = 0; i < 4; ++i) {
		// EmitVertex() snapshots every output and then leaves them undefined,
		// so the flat values have to be written again for each corner -- not
		// once up front.
		cameraSpherePos = vert[0].cameraSpherePos;
		sphereRadius = vert[0].sphereRadius;
		mapping = corners[i] * boxCorrection;

		vec4 cameraCornerPos = vec4(vert[0].cameraSpherePos, 1.0);
		cameraCornerPos.xy += mapping * vert[0].sphereRadius;
		gl_Position = cameraToClipMatrix * cameraCornerPos;

		// Which input primitive this came from -- the point's index in the
		// draw, which is the sphere's index in the material array.
		//
		// The book forwards this through the built-in: `gl_PrimitiveID =
		// gl_PrimitiveIDIn`, read as gl_PrimitiveID in the fragment shader.
		// That is what the spec says to do, and Apple's GL-on-Metal driver
		// ignores it: the fragment shader sees a counter of *emitted*
		// triangles instead, so each square's two halves get different
		// materials and the later spheres index off the end of the array.
		// A flat varying carries the same value and cannot be second-guessed.
		sphereIndex = gl_PrimitiveIDIn;

		EmitVertex();
	}
	// EndPrimitive() would start a second strip; with one strip per
	// invocation it is implied by the end of main().
}
