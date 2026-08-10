#pragma once

#include <glm/vec2.hpp>

#include <memory>
#include <string>
#include <string_view>

struct GLFWwindow;

// Windowing and context creation, split into three explicitly ordered pieces.
//
// humangl encoded this ordering purely in the declaration order of unnamed
// members, which worked but explained nothing. Here each step is a named type
// and the sequence is spelled out once, in App:
//
//     GlfwLibrary -> Window::create -> makeContextCurrent -> GlLoader
//
// Nothing before GlLoader may call a GL function, because until glewInit runs
// the function pointers are null.

namespace glc {

/// RAII owner of the GLFW library itself. Installs an error callback so GLFW's
/// own diagnostics reach the log instead of being discarded.
class GlfwLibrary {
public:
	GlfwLibrary();
	~GlfwLibrary();

	GlfwLibrary(const GlfwLibrary&) = delete;
	GlfwLibrary& operator=(const GlfwLibrary&) = delete;
	GlfwLibrary(GlfwLibrary&&) = delete;
	GlfwLibrary& operator=(GlfwLibrary&&) = delete;
};

struct WindowConfig {
	int width = 1280;
	int height = 720;
	std::string title = "gltut";

	/// Defaults come from the GLCORE_GL_VERSION cache variable (3.3 for gltut).
	/// macOS additionally requires the core profile and forward compatibility
	/// for anything above 3.2, which Window::create always requests.
	int glMajor = GLC_GL_MAJOR;
	int glMinor = GLC_GL_MINOR;

	int samples = 0; ///< MSAA sample count; 0 disables
	bool vsync = true;
	bool srgbFramebuffer = false; ///< Tutorial 16
	bool resizable = true;
};

class Window {
public:
	[[nodiscard]] static Window create(const WindowConfig& config);

	void makeContextCurrent() const;

	/// Requires a current context.
	void setVsync(bool enabled) const;

	[[nodiscard]] GLFWwindow* handle() const noexcept {
		return window_.get();
	}

	/// Pixels, not screen coordinates -- this is what glViewport wants, and the
	/// two differ on a Retina display.
	[[nodiscard]] glm::ivec2 framebufferSize() const;

	/// Screen coordinates.
	[[nodiscard]] glm::ivec2 size() const;

	[[nodiscard]] bool shouldClose() const;
	void setShouldClose(bool value) const;
	void swapBuffers() const;
	void setTitle(const std::string& title) const;

private:
	struct Deleter {
		void operator()(GLFWwindow* window) const noexcept;
	};

	explicit Window(GLFWwindow* window) noexcept : window_{window} {}

	std::unique_ptr<GLFWwindow, Deleter> window_;
};

/// Initializes GLEW against the current context. Must be constructed after
/// Window::makeContextCurrent() and before any other GL call.
class GlLoader {
public:
	[[nodiscard]] static GlLoader initForCurrentContext();

	[[nodiscard]] std::string_view vendor() const;
	[[nodiscard]] std::string_view renderer() const;
	[[nodiscard]] std::string_view version() const;
	[[nodiscard]] std::string_view glslVersion() const;

	/// Logs vendor/renderer/version at info level.
	void logSummary() const;

private:
	GlLoader() = default;
};

} // namespace glc
