#pragma once

#include <glcore/gl.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

// GL error checking.
//
// macOS caps OpenGL at 4.1, so glDebugMessageCallback / KHR_debug (4.3) is not
// available and there is no way to get asynchronous driver diagnostics. The
// only mechanism left is glGetError, so we make it ergonomic:
//
//   GLC_CHECK(glDrawArrays(GL_TRIANGLES, 0, 3));
//
// In a Debug build that runs the call and throws GlError naming the exact
// expression, file and line if the error queue is non-empty. In a release build
// it compiles down to the bare call with no overhead.
//
// Wrapping every single GL call is not the intent -- wrap the ones that fail
// interestingly (draws, uploads, framebuffer setup). FrameErrorScope catches
// everything else once per frame in every build for roughly the cost of one
// glGetError.

namespace glc {

class GlError : public std::runtime_error {
public:
	explicit GlError(const std::string& message) : std::runtime_error(message) {}
};

/// Human-readable name for a GL error enum, e.g. "GL_INVALID_OPERATION".
[[nodiscard]] std::string_view glErrorName(GLenum error);

/// Empties the error queue. Returns a description of everything found, or an
/// empty string if the queue was clean. Never throws.
[[nodiscard]] std::string drainGlErrors();

/// Empties the error queue and logs anything found as an error. Returns the
/// number of distinct errors drained.
///
/// noexcept so that FrameErrorScope's destructor genuinely cannot throw.
int checkGlErrors(std::string_view context) noexcept;

/// Drains the queue once at end of scope. Used by the frame loop.
class FrameErrorScope {
public:
	explicit FrameErrorScope(std::string_view context) noexcept : context_{context} {}
	~FrameErrorScope();

	FrameErrorScope(const FrameErrorScope&) = delete;
	FrameErrorScope& operator=(const FrameErrorScope&) = delete;
	FrameErrorScope(FrameErrorScope&&) = delete;
	FrameErrorScope& operator=(FrameErrorScope&&) = delete;

private:
	std::string_view context_;
};

namespace detail {
/// Throws GlError if the queue is non-empty. Used by GLC_CHECK.
void throwOnGlError(std::string_view expression, std::string_view file, int line);
} // namespace detail

} // namespace glc

#ifdef GLC_DEBUG
#define GLC_CHECK(expr)                                                                            \
	do {                                                                                           \
		expr;                                                                                      \
		::glc::detail::throwOnGlError(#expr, __FILE__, __LINE__);                                  \
	} while (false)
#else
#define GLC_CHECK(expr)                                                                            \
	do {                                                                                           \
		expr;                                                                                      \
	} while (false)
#endif
