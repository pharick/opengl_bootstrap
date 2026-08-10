#include <glcore/gl_check.hpp>

#include <glcore/log.hpp>

#include <format>

namespace glc {
namespace {

// A GL implementation may queue several errors; bound the loop so a driver that
// never returns GL_NO_ERROR cannot hang the frame.
constexpr int kMaxErrorsPerDrain = 16;

} // namespace

std::string_view glErrorName(GLenum error) {
	switch (error) {
		case GL_NO_ERROR:
			return "GL_NO_ERROR";
		case GL_INVALID_ENUM:
			return "GL_INVALID_ENUM";
		case GL_INVALID_VALUE:
			return "GL_INVALID_VALUE";
		case GL_INVALID_OPERATION:
			return "GL_INVALID_OPERATION";
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			return "GL_INVALID_FRAMEBUFFER_OPERATION";
		case GL_OUT_OF_MEMORY:
			return "GL_OUT_OF_MEMORY";
		case GL_STACK_UNDERFLOW:
			return "GL_STACK_UNDERFLOW";
		case GL_STACK_OVERFLOW:
			return "GL_STACK_OVERFLOW";
		default:
			return "GL_UNKNOWN_ERROR";
	}
}

std::string drainGlErrors() {
	std::string result;
	for (int i = 0; i < kMaxErrorsPerDrain; ++i) {
		const GLenum error = glGetError();
		if (error == GL_NO_ERROR) {
			break;
		}
		if (!result.empty()) {
			result += ", ";
		}
		result += std::format("{} (0x{:04X})", glErrorName(error), error);
	}
	return result;
}

int checkGlErrors(std::string_view context) noexcept {
	int count = 0;
	for (int i = 0; i < kMaxErrorsPerDrain; ++i) {
		const GLenum error = glGetError();
		if (error == GL_NO_ERROR) {
			break;
		}
		++count;
		log::error("GL error in {}: {} (0x{:04X})", context, glErrorName(error), error);
	}
	return count;
}

FrameErrorScope::~FrameErrorScope() {
	checkGlErrors(context_);
}

void detail::throwOnGlError(std::string_view expression, std::string_view file, int line) {
	const std::string errors = drainGlErrors();
	if (errors.empty()) {
		return;
	}
	throw GlError(std::format("{}:{}: {} -> {}", file, line, expression, errors));
}

} // namespace glc
