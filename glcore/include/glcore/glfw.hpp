#pragma once

// GLFW, included in the one order that works.
//
// GLEW must be seen before GLFW pulls in the system GL headers, so anything
// needing GLFW_* constants includes this rather than <GLFW/glfw3.h> directly.

#include <glcore/gl.hpp>
// clang-format off
#include <GLFW/glfw3.h>
// clang-format on
