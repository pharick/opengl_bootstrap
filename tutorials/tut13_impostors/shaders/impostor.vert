#version 330

// A camera-facing square, and no vertex attributes at all.
//
// The VAO this runs over is empty. There is no buffer; each of the four corners
// is picked by gl_VertexID, which the driver supplies for free with
// glDrawArrays(GL_TRIANGLE_STRIP, 0, 4). The square is built in camera space,
// where "facing the camera" is simply "lying in the XY plane", and then
// projected like anything else.
//
// The corner order (bottom-left, top-left, bottom-right, top-right) makes the
// strip's first triangle wind clockwise on screen, which is the front face for
// gltut's meshes and therefore for this program's cull state too.
//
// Shared by every impostor fragment shader in this chapter. They differ in how
// much square they need: the basic one wants exactly the sphere's bounding
// box, the ray-traced one wants more, because under perspective a sphere's
// outline is an ellipse that spills past the box on the side away from the
// screen centre. boxCorrection scales both the square and the mapping, so a
// fragment's mapping still measures sphere radii from the centre.

layout(std140) uniform;

out vec2 mapping;

uniform Projection {
	mat4 cameraToClipMatrix;
};

uniform float sphereRadius;
uniform vec3 cameraSpherePos;
uniform float boxCorrection;

void main() {
	vec2 corner;
	switch (gl_VertexID) {
	case 0:
		corner = vec2(-1.0, -1.0);
		break;
	case 1:
		corner = vec2(-1.0, 1.0);
		break;
	case 2:
		corner = vec2(1.0, -1.0);
		break;
	case 3:
		corner = vec2(1.0, 1.0);
		break;
	}

	mapping = corner * boxCorrection;

	vec4 cameraCornerPos = vec4(cameraSpherePos, 1.0);
	cameraCornerPos.xy += mapping * sphereRadius;

	gl_Position = cameraToClipMatrix * cameraCornerPos;
}
