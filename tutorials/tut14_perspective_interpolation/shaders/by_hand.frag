#version 330

// The other half of the by-hand correction: two window-space lerps come in,
// one divide gets the camera-space value back out.
//
// (gl_FragCoord.w is 1 / W interpolated exactly this way, so oneOverW could
// have been read from there. Carrying it explicitly keeps the trick visible.)

noperspective in vec4 colorOverW;
noperspective in float oneOverW;

out vec4 outputColor;

void main() {
	outputColor = colorOverW / oneOverW;
}
