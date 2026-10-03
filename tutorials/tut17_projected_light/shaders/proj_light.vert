#version 330

// The book's projLight.vert: litTexture.vert plus one more transform.
//
// cameraToLightProjMatrix takes a view-camera-space position all the way into
// the flashlight's texture space: back to world space, into the light's camera
// space, through its perspective projection, and finally from [-1, 1] to
// [0, 1]. That last step is a post-projective transform -- it is applied
// before the divide, which the fragment shader leaves to textureProj.
//
// Doing this per vertex is fine: a projection is linear in homogeneous
// coordinates, so interpolating the undivided vec4 and dividing per fragment
// gives the right answer. Only the divide must wait for the fragment.

layout(std140) uniform;

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec3 inNormal;
layout(location = 5) in vec2 inTexCoord;

out vec2 colorCoord;
out vec3 cameraSpacePosition;
out vec3 cameraSpaceNormal;
out vec4 lightProjPosition;

uniform Projection {
	mat4 cameraToClipMatrix;
};

uniform mat4 modelToCameraMatrix;
uniform mat3 normalModelToCameraMatrix;
uniform mat4 cameraToLightProjMatrix;

void main() {
	cameraSpacePosition = (modelToCameraMatrix * vec4(inPosition, 1.0)).xyz;
	gl_Position = cameraToClipMatrix * vec4(cameraSpacePosition, 1.0);
	cameraSpaceNormal = normalize(normalModelToCameraMatrix * inNormal);
	lightProjPosition = cameraToLightProjMatrix * vec4(cameraSpacePosition, 1.0);
	colorCoord = inTexCoord;
}
