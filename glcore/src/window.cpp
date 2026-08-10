#include <glcore/window.hpp>

#include <glcore/gl.hpp> // GLEW first
// clang-format off
#include <GLFW/glfw3.h>
// clang-format on

#include <glcore/gl_check.hpp>
#include <glcore/log.hpp>

#include <format>
#include <stdexcept>

namespace glc {
namespace {

std::string_view glString(GLenum name) {
	const GLubyte* value = glGetString(name);
	return value == nullptr ? std::string_view{"<unavailable>"}
	                        : std::string_view{reinterpret_cast<const char*>(value)};
}

void onGlfwError(int code, const char* description) {
	log::error("GLFW error {}: {}", code,
	           description == nullptr ? "(no description)" : description);
}

} // namespace

// ---------------------------------------------------------------------------
// GlfwLibrary
// ---------------------------------------------------------------------------

GlfwLibrary::GlfwLibrary() {
	// Installed before glfwInit so failures during init are reported too.
	glfwSetErrorCallback(&onGlfwError);
	if (glfwInit() != GLFW_TRUE) {
		throw std::runtime_error("failed to initialize GLFW");
	}
}

GlfwLibrary::~GlfwLibrary() {
	glfwTerminate();
}

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------

void Window::Deleter::operator()(GLFWwindow* window) const noexcept {
	if (window != nullptr) {
		glfwDestroyWindow(window);
	}
}

Window Window::create(const WindowConfig& config) {
	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, config.glMajor);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, config.glMinor);

	// macOS only ever offers core-profile, forward-compatible contexts above
	// 3.2. Requesting them unconditionally is also the portable choice.
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

	glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
	glfwWindowHint(GLFW_SAMPLES, config.samples);
	glfwWindowHint(GLFW_SRGB_CAPABLE, config.srgbFramebuffer ? GLFW_TRUE : GLFW_FALSE);
	glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);

	GLFWwindow* raw =
	    glfwCreateWindow(config.width, config.height, config.title.c_str(), nullptr, nullptr);
	if (raw == nullptr) {
		throw std::runtime_error(std::format(
		    "failed to create a {}.{} core-profile window -- this machine may not support it",
		    config.glMajor, config.glMinor));
	}

	return Window{raw};
}

void Window::makeContextCurrent() const {
	glfwMakeContextCurrent(window_.get());
}

void Window::setVsync(bool enabled) const {
	glfwSwapInterval(enabled ? 1 : 0);
}

glm::ivec2 Window::framebufferSize() const {
	glm::ivec2 size{0, 0};
	glfwGetFramebufferSize(window_.get(), &size.x, &size.y);
	return size;
}

glm::ivec2 Window::size() const {
	glm::ivec2 size{0, 0};
	glfwGetWindowSize(window_.get(), &size.x, &size.y);
	return size;
}

bool Window::shouldClose() const {
	return glfwWindowShouldClose(window_.get()) == GLFW_TRUE;
}

void Window::setShouldClose(bool value) const {
	glfwSetWindowShouldClose(window_.get(), value ? GLFW_TRUE : GLFW_FALSE);
}

void Window::swapBuffers() const {
	glfwSwapBuffers(window_.get());
}

void Window::setTitle(const std::string& title) const {
	glfwSetWindowTitle(window_.get(), title.c_str());
}

// ---------------------------------------------------------------------------
// GlLoader
// ---------------------------------------------------------------------------

GlLoader GlLoader::initForCurrentContext() {
	if (glfwGetCurrentContext() == nullptr) {
		throw std::runtime_error("no current GL context -- call Window::makeContextCurrent first");
	}

	// Required for core profiles: without it GLEW skips entry points it thinks
	// are unavailable because glGetString(GL_EXTENSIONS) is gone in core.
	glewExperimental = GL_TRUE;

	if (const GLenum status = glewInit(); status != GLEW_OK) {
		throw std::runtime_error(
		    std::format("failed to initialize GLEW: {}",
		                reinterpret_cast<const char*>(glewGetErrorString(status))));
	}

	// glewInit itself calls glGetString(GL_EXTENSIONS), which is illegal in a
	// core profile and leaves a GL_INVALID_ENUM behind. Discard it so the first
	// real GLC_CHECK does not blame the caller for GLEW's own mistake.
	if (const std::string spurious = drainGlErrors(); !spurious.empty()) {
		log::trace("discarded GLEW's spurious init error: {}", spurious);
	}

	return GlLoader{};
}

std::string_view GlLoader::vendor() const {
	return glString(GL_VENDOR);
}

std::string_view GlLoader::renderer() const {
	return glString(GL_RENDERER);
}

std::string_view GlLoader::version() const {
	return glString(GL_VERSION);
}

std::string_view GlLoader::glslVersion() const {
	return glString(GL_SHADING_LANGUAGE_VERSION);
}

void GlLoader::logSummary() const {
	log::info("GL {} | GLSL {}", version(), glslVersion());
	log::info("{} / {}", vendor(), renderer());
}

} // namespace glc
