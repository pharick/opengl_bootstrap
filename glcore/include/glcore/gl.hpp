#pragma once

// Single point of inclusion for the OpenGL API.
//
// GLEW must be included before any other GL header, so everything in glcore
// (and every tutorial) goes through this file rather than including <GL/glew.h>
// directly. On macOS the entire GL API carries a deprecation attribute; the
// build defines GL_SILENCE_DEPRECATION publicly on the glcore target.

#include <GL/glew.h>
