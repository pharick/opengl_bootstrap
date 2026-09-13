// Depth for a fragment whose true surface is not where the rasterizer thinks
// it is. Shared by impostor_depth.frag and impostor_geom.frag.
//
// Declares the Projection block, since it needs the projection matrix. A
// fragment shader including this therefore shares the block with its vertex
// (or geometry) stage; the definitions must match, and they do.

#ifndef TUT13_FRAG_DEPTH_GLSL
#define TUT13_FRAG_DEPTH_GLSL

layout(std140) uniform;

uniform Projection {
	mat4 cameraToClipMatrix;
};

/// What gl_FragCoord.z would have been for a fragment at `cameraPos`: the
/// pipeline the rasterizer runs on every vertex, replayed for one point.
/// Camera space -> clip space (projection matrix) -> NDC (divide by w) ->
/// window space (the glDepthRange mapping, which GLSL exposes as
/// gl_DepthRange).
float windowDepth(in vec3 cameraPos) {
	vec4 clipPos = cameraToClipMatrix * vec4(cameraPos, 1.0);
	float ndcDepth = clipPos.z / clipPos.w;

	// glDepthRange(near, far) maps NDC [-1, 1] onto [near, far]. Written the
	// way the spec states it, with diff = far - near.
	return ((gl_DepthRange.diff * ndcDepth) + gl_DepthRange.near + gl_DepthRange.far) / 2.0;
}

#endif
