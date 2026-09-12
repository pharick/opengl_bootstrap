#pragma once

#include <glcore/camera.hpp>
#include <glcore/frame_timer.hpp>
#include <glcore/gl.hpp>
#include <glcore/gl_check.hpp>
#include <glcore/imgui_layer.hpp>
#include <glcore/input.hpp>
#include <glcore/log.hpp>
#include <glcore/matrix_stack.hpp>
#include <glcore/shader_watcher.hpp>
#include <glcore/window.hpp>

#include <glm/vec4.hpp>

#include <cstdlib>
#include <exception>
#include <optional>
#include <utility>

namespace glc {

struct AppConfig {
	WindowConfig window{};
	bool depthTest = true;
	bool cullFace = false;

	/// Winding order that counts as front-facing, when cullFace is on. gltut's
	/// XML meshes are wound clockwise, so a chapter loading those wants GL_CW --
	/// otherwise culling keeps the wrong half of every double-sided surface.
	GLenum frontFace = GL_CCW;

	/// Create a Dear ImGui context and call onGui() every frame.
	bool imgui = true;

	/// Draw the built-in frame-time / shader-reload overlay. Needs imgui.
	bool showStats = true;

	/// Clear the framebuffer before onRender(). Set false to issue glClear
	/// yourself, as the gltut listings do.
	bool autoClear = true;
	glm::vec4 clearColor{0.08F, 0.09F, 0.11F, 1.0F};

	/// Largest frame delta handed to onUpdate, in seconds.
	float maxDelta = 0.1F;

	/// Close the window when Escape is pressed.
	bool escapeToQuit = true;

	/// Exit after this many frames; 0 runs until the window is closed. The
	/// GLC_MAX_FRAMES environment variable overrides it, which makes any
	/// tutorial usable as a smoke test under a sanitizer or in CI:
	///
	///     GLC_MAX_FRAMES=60 ./build/asan/bin/tut01_hello_triangle
	unsigned long long maxFrames = 0;
};

/// Base class for a tutorial.
///
/// gltut's own framework owns main() and calls into per-chapter code; this
/// mirrors that, so listings from the book drop into onInit/onRender with
/// little translation.
///
/// Resource lifetime works out for free: GL objects held as members of your
/// derived class are destroyed before this base subobject, so the context is
/// still current when their destructors run.
class App {
public:
	explicit App(AppConfig config = {});
	virtual ~App();

	App(const App&) = delete;
	App& operator=(const App&) = delete;
	App(App&&) = delete;
	App& operator=(App&&) = delete;

	void run();

protected:
	/// Called once, after the context exists and before the first frame.
	/// Deliberately not called from the constructor -- virtual dispatch does not
	/// reach a derived override during base construction.
	virtual void onInit() {}

	virtual void onUpdate(float /*deltaSeconds*/) {}
	virtual void onRender() = 0;
	virtual void onResize(int /*width*/, int /*height*/) {}
	virtual void onKey(int /*key*/, int /*scancode*/, int /*action*/, int /*mods*/) {}

	/// Called inside an ImGui frame. Put ImGui::Begin/End widgets here.
	virtual void onGui() {}

	/// Programs registered here are watched for edits and rebuilt automatically.
	[[nodiscard]] ShaderWatcher& shaders() noexcept {
		return shaders_;
	}

	/// Keyboard and mouse for this frame. Reports neutral values while ImGui
	/// has the input.
	[[nodiscard]] const Input& input() const noexcept {
		return input_;
	}

	/// Aspect ratio is kept in step with the framebuffer automatically; the
	/// view matrix is yours to set, directly or via a controller.
	[[nodiscard]] Camera& camera() noexcept {
		return camera_;
	}
	[[nodiscard]] const Camera& camera() const noexcept {
		return camera_;
	}

	/// A transform stack, for chapters 6 and up.
	[[nodiscard]] MatrixStack& matrices() noexcept {
		return matrices_;
	}

	[[nodiscard]] Window& window() noexcept {
		return window_;
	}
	[[nodiscard]] const Window& window() const noexcept {
		return window_;
	}
	[[nodiscard]] const FrameTimer& timer() const noexcept {
		return timer_;
	}
	[[nodiscard]] const AppConfig& config() const noexcept {
		return config_;
	}
	[[nodiscard]] glm::ivec2 framebufferSize() const {
		return window_.framebufferSize();
	}
	[[nodiscard]] float aspect() const;

	void requestClose() {
		window_.setShouldClose(true);
	}

private:
	void installCallbacks();
	void applyInitialGlState() const;
	void clear() const;
	void drawStatsOverlay();

	void handleFramebufferSize(int width, int height);
	void handleKey(int key, int scancode, int action, int mods);
	void handleMouseButton(int button, int action, int mods);
	void handleCursorPos(double x, double y);
	void handleScroll(double xOffset, double yOffset);

	friend struct AppCallbacks;

	// Declaration order IS the initialisation order, and it matters:
	// config_ feeds window creation; GLFW must be alive before a window exists;
	// the context must be current before GLEW loads any entry point.
	//
	// Destruction runs in reverse, so imgui_ and shaders_ release their GL
	// objects while the context is still alive.
	AppConfig config_;
	GlfwLibrary glfw_;
	Window window_;
	GlLoader loader_;
	std::optional<ImGuiLayer> imgui_;
	ShaderWatcher shaders_;
	FrameTimer timer_;
	Input input_;
	Camera camera_;
	MatrixStack matrices_;
};

/// Constructs and runs an App subclass, turning any escaping exception into a
/// logged message and a non-zero exit code.
template<class T, class... Args>
int runApp(Args&&... args) {
	try {
		T app{std::forward<Args>(args)...};
		app.run();
	} catch (const std::exception& error) {
		log::error("fatal: {}", error.what());
		return EXIT_FAILURE;
	} catch (...) {
		log::error("fatal: unknown exception");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

} // namespace glc
