#include <glcore/app.hpp>

#include <glcore/gl.hpp> // GLEW first
// clang-format off
#include <GLFW/glfw3.h>
// clang-format on

#include <imgui.h>

#include <charconv>
#include <cstdlib>
#include <string_view>
#include <system_error>

namespace glc {
namespace {

/// Reads GLC_MAX_FRAMES, if set to a positive integer.
unsigned long long frameLimitFromEnvironment(unsigned long long fallback) {
	const char* value = std::getenv("GLC_MAX_FRAMES");
	if (value == nullptr) {
		return fallback;
	}
	const std::string_view text{value};
	unsigned long long parsed = 0;
	const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), parsed);
	if (ec != std::errc{} || ptr != text.data() + text.size()) {
		log::warn("ignoring GLC_MAX_FRAMES='{}': not a number", text);
		return fallback;
	}
	return parsed;
}

/// Bridges the two steps that must sit between window creation and any GL call.
GlLoader makeContextAndLoad(const Window& window, const WindowConfig& config) {
	window.makeContextCurrent();
	GlLoader loader = GlLoader::initForCurrentContext();
	window.setVsync(config.vsync); // needs a current context
	return loader;
}

} // namespace

/// Trampolines from GLFW's C callbacks back to the owning App.
struct AppCallbacks {
	static App* appFor(GLFWwindow* window) {
		return static_cast<App*>(glfwGetWindowUserPointer(window));
	}

	static void framebufferSize(GLFWwindow* window, int width, int height) {
		if (App* app = appFor(window); app != nullptr) {
			app->handleFramebufferSize(width, height);
		}
	}

	static void key(GLFWwindow* window, int key, int scancode, int action, int mods) {
		if (App* app = appFor(window); app != nullptr) {
			app->handleKey(key, scancode, action, mods);
		}
	}

	static void mouseButton(GLFWwindow* window, int button, int action, int mods) {
		if (App* app = appFor(window); app != nullptr) {
			app->handleMouseButton(button, action, mods);
		}
	}

	static void cursorPos(GLFWwindow* window, double x, double y) {
		if (App* app = appFor(window); app != nullptr) {
			app->handleCursorPos(x, y);
		}
	}

	static void scroll(GLFWwindow* window, double xOffset, double yOffset) {
		if (App* app = appFor(window); app != nullptr) {
			app->handleScroll(xOffset, yOffset);
		}
	}
};

App::App(AppConfig config)
    : config_{std::move(config)}, window_{Window::create(config_.window)},
      loader_{makeContextAndLoad(window_, config_.window)} {
	loader_.logSummary();
	timer_.setMaxDelta(config_.maxDelta);
	config_.maxFrames = frameLimitFromEnvironment(config_.maxFrames);
}

App::~App() = default;

float App::aspect() const {
	const glm::ivec2 size = window_.framebufferSize();
	return size.y > 0 ? static_cast<float>(size.x) / static_cast<float>(size.y) : 1.0F;
}

void App::installCallbacks() {
	glfwSetWindowUserPointer(window_.handle(), this);
	glfwSetFramebufferSizeCallback(window_.handle(), &AppCallbacks::framebufferSize);
	glfwSetKeyCallback(window_.handle(), &AppCallbacks::key);
	glfwSetMouseButtonCallback(window_.handle(), &AppCallbacks::mouseButton);
	glfwSetCursorPosCallback(window_.handle(), &AppCallbacks::cursorPos);
	glfwSetScrollCallback(window_.handle(), &AppCallbacks::scroll);
}

void App::applyInitialGlState() const {
	if (config_.depthTest) {
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_TRUE);
	}
	if (config_.cullFace) {
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);
	}
	if (config_.window.samples > 0) {
		glEnable(GL_MULTISAMPLE);
	}
	if (config_.window.srgbFramebuffer) {
		glEnable(GL_FRAMEBUFFER_SRGB);
	}
}

void App::clear() const {
	const glm::vec4& color = config_.clearColor;
	glClearColor(color.r, color.g, color.b, color.a);

	GLbitfield mask = GL_COLOR_BUFFER_BIT;
	if (config_.depthTest) {
		mask |= GL_DEPTH_BUFFER_BIT;
	}
	glClear(mask);
}

void App::handleFramebufferSize(int width, int height) {
	glViewport(0, 0, width, height);
	if (width > 0 && height > 0) {
		camera_.setAspect(static_cast<float>(width) / static_cast<float>(height));
		onResize(width, height);
	}
}

void App::handleKey(int key, int scancode, int action, int mods) {
	input_.onKey(key, action);
	if (config_.escapeToQuit && key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		requestClose();
	}
	onKey(key, scancode, action, mods);
}

void App::handleMouseButton(int button, int action, int /*mods*/) {
	input_.onMouseButton(button, action);
}

void App::handleCursorPos(double x, double y) {
	input_.onCursorPos(x, y);
}

void App::handleScroll(double xOffset, double yOffset) {
	input_.onScroll(xOffset, yOffset);
}

void App::drawStatsOverlay() {
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 10.0F, viewport->WorkPos.y + 10.0F),
	                        ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowBgAlpha(0.5F);

	constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
	                                   ImGuiWindowFlags_AlwaysAutoResize |
	                                   ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

	if (ImGui::Begin("stats", nullptr, flags)) {
		ImGui::Text("%.2f ms  (%.0f fps)", static_cast<double>(timer_.smoothedFrameMs()),
		            static_cast<double>(timer_.smoothedFps()));
		const glm::ivec2 size = window_.framebufferSize();
		ImGui::Text("%d x %d framebuffer", size.x, size.y);
		if (shaders_.size() > 0) {
			ImGui::Separator();
			ImGui::Text("%zu watched program(s), %d reload(s)", shaders_.size(),
			            shaders_.reloadCount());
			if (ImGui::Button("Reload shaders")) {
				shaders_.reloadAll();
			}
		}
	}
	ImGui::End();
}

void App::run() {
	installCallbacks();
	applyInitialGlState();

	// After installCallbacks: ImGui's GLFW backend chains to the callbacks it
	// finds already installed, so both it and the tutorial receive input.
	if (config_.imgui) {
		imgui_.emplace(window_);
	}

	onInit();

	// Prime the viewport and give the tutorial its first size before frame one.
	const glm::ivec2 size = window_.framebufferSize();
	handleFramebufferSize(size.x, size.y);

	checkGlErrors("initialisation");

	while (!window_.shouldClose()) {
		// Edge flags and deltas are cleared before polling, so the events GLFW
		// dispatches during this call land in a clean frame.
		input_.beginFrame();
		glfwPollEvents();

		input_.setMouseCaptured(imgui_.has_value() && ImGuiLayer::wantsMouse());
		input_.setKeyboardCaptured(imgui_.has_value() && ImGuiLayer::wantsKeyboard());

		shaders_.poll();

		const float delta = timer_.tick();
		onUpdate(delta);

		if (imgui_) {
			imgui_->beginFrame();
			if (config_.showStats) {
				drawStatsOverlay();
			}
			onGui();
		}

		{
			const FrameErrorScope scope{"frame"};
			if (config_.autoClear) {
				clear();
			}
			onRender();
		}

		// After onRender so the UI draws on top of the scene.
		if (imgui_) {
			imgui_->endFrame();
		}

		window_.swapBuffers();

		if (config_.maxFrames != 0 && timer_.frameCount() >= config_.maxFrames) {
			log::info("frame limit of {} reached; exiting", config_.maxFrames);
			requestClose();
		}
	}
}

} // namespace glc
