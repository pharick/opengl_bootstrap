#version 330

// Position, normal and texture coordinate -- the book's PNT.vert.
//
// This is texture mapping: the association between points on a triangle and
// texels in a texture, carried as one more per-vertex attribute. The vertex
// shader has nothing to compute for it. It passes the coordinate through, and
// the rasterizer interpolates it across the triangle the way it interpolates
// everything else -- perspective-correctly, as the previous example showed --
// so each fragment lands on the texel that its position on the surface maps
// to.
//
// Attribute 5 is gltut's slot for texture coordinates; the mesh files agree.

layout(std140) uniform;

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec3 inNormal;
layout(location = 5) in vec2 inTexCoord;

out vec3 vertexNormal;
out vec3 cameraSpacePosition;
out vec2 shinTexCoord;

uniform Projection {
	mat4 cameraToClipMatrix;
};

uniform mat4 modelToCameraMatrix;
uniform mat3 normalModelToCameraMatrix;

void main() {
	vec4 cameraPosition = modelToCameraMatrix * vec4(inPosition, 1.0);
	gl_Position = cameraToClipMatrix * cameraPosition;

	vertexNormal = normalModelToCameraMatrix * inNormal;
	cameraSpacePosition = vec3(cameraPosition);

	shinTexCoord = inTexCoord;
}
