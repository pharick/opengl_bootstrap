#version 330

// Perspective-correct interpolation done by hand. Not in the book; it is here
// to show what the rasterizer does for a `smooth` output.
//
// Both outputs are `noperspective`, so they are lerped in window space just
// like the linear program's color. The trick is *what* is lerped: color / W
// and 1 / W. Neither is linear in camera space, but the perspective divide
// makes both of them linear in window space -- that is the one thing the
// divide preserves -- so the rasterizer's flat lerp gets them exactly right,
// and the fragment shader divides one by the other to recover the color.
//
//     color(p) = lerp(color_i / w_i) / lerp(1 / w_i)
//
// This is the correction the hardware applies. Notice its only input beyond
// the attribute itself: gl_Position.w. A mesh whose vertices all share one W
// -- the faux hallway -- gets nothing from it, and this program collapses to
// the linear one.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

noperspective out vec4 colorOverW;
noperspective out float oneOverW;

uniform mat4 cameraToClipMatrix;

void main() {
	gl_Position = cameraToClipMatrix * vec4(inPosition, 1.0);
	colorOverW = inColor / gl_Position.w;
	oneOverW = 1.0 / gl_Position.w;
}
