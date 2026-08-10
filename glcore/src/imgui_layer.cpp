#include <glcore/imgui_layer.hpp>

#include <glcore/gl.hpp> // GLEW first
// clang-format off
#include <GLFW/glfw3.h>
// clang-format on

#include <glcore/log.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <format>
#include <stdexcept>

namespace glc {
namespace {

/// The backend needs a #version string valid in *this* context. ImGui's own
/// default is not valid in a core forward-compatible profile, so derive it from
/// the version the project targets.
std::string glslVersionDirective() {
	return std::format("#version {}{}0", GLC_GL_MAJOR, GLC_GL_MINOR);
}

} // namespace

ImGuiLayer::ImGuiLayer(const Window& window) {
	IMGUI_CHECKVERSION();
	if (ImGui::CreateContext() == nullptr) {
		throw std::runtime_error("failed to create the ImGui context");
	}

	ImGui::StyleColorsDark();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	if (!ImGui_ImplGlfw_InitForOpenGL(window.handle(), true)) {
		ImGui::DestroyContext();
		throw std::runtime_error("failed to initialize the ImGui GLFW backend");
	}

	const std::string glsl = glslVersionDirective();
	if (!ImGui_ImplOpenGL3_Init(glsl.c_str())) {
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		throw std::runtime_error(
		    std::format("failed to initialize the ImGui OpenGL3 backend with '{}'", glsl));
	}
}

ImGuiLayer::~ImGuiLayer() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void ImGuiLayer::beginFrame() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void ImGuiLayer::endFrame() {
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool ImGuiLayer::wantsMouse() {
	return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiLayer::wantsKeyboard() {
	return ImGui::GetIO().WantCaptureKeyboard;
}

} // namespace glc
