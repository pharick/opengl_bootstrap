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

layout(std140) uniform;

out vec2 mapping;

uniform Projection {
	mat4 cameraToClipMatrix;
};

uniform float sphereRadius;
uniform vec3 cameraSpherePos;

void main() {
	vec2 offset;
	switch (gl_VertexID) {
	case 0:
		mapping = vec2(-1.0, -1.0);
		offset = vec2(-sphereRadius, -sphereRadius);
		break;
	case 1:
		mapping = vec2(-1.0, 1.0);
		offset = vec2(-sphereRadius, sphereRadius);
		break;
	case 2:
		mapping = vec2(1.0, -1.0);
		offset = vec2(sphereRadius, -sphereRadius);
		break;
	case 3:
		mapping = vec2(1.0, 1.0);
		offset = vec2(sphereRadius, sphereRadius);
		break;
	}

	vec4 cameraCornerPos = vec4(cameraSpherePos, 1.0);
	cameraCornerPos.xy += offset;

	gl_Position = cameraToClipMatrix * cameraCornerPos;
}
