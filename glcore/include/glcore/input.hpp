#pragma once

#include <glcore/glfw.hpp>

#include <glm/vec2.hpp>

#include <array>
#include <cstdint>

// Per-frame keyboard and mouse state.
//
// GLFW is event-driven; this turns it into the polled form that game loops
// want, including edge detection ("was it pressed *this* frame") which
// glfwGetKey cannot answer.
//
// Use GLFW's own constants: input.keyDown(GLFW_KEY_W).

namespace glc {

class Input {
public:
	[[nodiscard]] bool keyDown(int key) const;
	/// True only on the frame the key went down.
	[[nodiscard]] bool keyPressed(int key) const;
	/// True only on the frame the key came up.
	[[nodiscard]] bool keyReleased(int key) const;

	[[nodiscard]] bool mouseDown(int button) const;
	[[nodiscard]] bool mousePressed(int button) const;
	[[nodiscard]] bool mouseReleased(int button) const;

	[[nodiscard]] glm::vec2 mousePosition() const noexcept {
		return mousePosition_;
	}
	/// Movement since the previous frame, in screen coordinates.
	[[nodiscard]] glm::vec2 mouseDelta() const noexcept;
	[[nodiscard]] float scrollDelta() const noexcept;

	/// When ImGui owns the input, these report neutral values so a camera
	/// controller does not also react to a slider drag.
	void setMouseCaptured(bool captured) noexcept {
		mouseCaptured_ = captured;
	}
	void setKeyboardCaptured(bool captured) noexcept {
		keyboardCaptured_ = captured;
	}
	[[nodiscard]] bool mouseCaptured() const noexcept {
		return mouseCaptured_;
	}
	[[nodiscard]] bool keyboardCaptured() const noexcept {
		return keyboardCaptured_;
	}

	// --- driven by App; not part of the tutorial-facing API ---------------
	/// Clears edge flags and deltas. Must run before glfwPollEvents.
	void beginFrame();
	void onKey(int key, int action);
	void onMouseButton(int button, int action);
	void onCursorPos(double x, double y);
	void onScroll(double xOffset, double yOffset);

private:
	static constexpr std::size_t kKeyCount = GLFW_KEY_LAST + 1;
	static constexpr std::size_t kButtonCount = GLFW_MOUSE_BUTTON_LAST + 1;

	struct State {
		bool down = false;
		bool pressed = false;
		bool released = false;
	};

	[[nodiscard]] static bool inRange(int index, std::size_t count);

	std::array<State, kKeyCount> keys_{};
	std::array<State, kButtonCount> buttons_{};

	glm::vec2 mousePosition_{0.0F, 0.0F};
	glm::vec2 mouseDelta_{0.0F, 0.0F};
	float scrollDelta_ = 0.0F;
	bool hasCursorSample_ = false;

	bool mouseCaptured_ = false;
	bool keyboardCaptured_ = false;
};

} // namespace glc
