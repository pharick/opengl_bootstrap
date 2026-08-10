#pragma once

#include <glcore/window.hpp>

namespace glc {

/// RAII owner of the Dear ImGui context and its two backends.
///
/// Construct it *after* App has installed its own GLFW callbacks: the GLFW
/// backend chains to whatever was previously installed, so this ordering lets
/// both ImGui and the tutorial see input events.
class ImGuiLayer {
public:
	explicit ImGuiLayer(const Window& window);
	~ImGuiLayer();

	ImGuiLayer(const ImGuiLayer&) = delete;
	ImGuiLayer& operator=(const ImGuiLayer&) = delete;
	ImGuiLayer(ImGuiLayer&&) = delete;
	ImGuiLayer& operator=(ImGuiLayer&&) = delete;

	void beginFrame();
	void endFrame();

	/// True when ImGui is using the input, so the tutorial should ignore it --
	/// otherwise dragging a slider also spins the camera.
	[[nodiscard]] static bool wantsMouse();
	[[nodiscard]] static bool wantsKeyboard();
};

} // namespace glc
