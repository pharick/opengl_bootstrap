#include <glcore/input.hpp>

#include <utility>

namespace glc {

bool Input::inRange(int index, std::size_t count) {
	return index >= 0 && std::cmp_less(index, count);
}

void Input::beginFrame() {
	for (State& state : keys_) {
		state.pressed = false;
		state.released = false;
	}
	for (State& state : buttons_) {
		state.pressed = false;
		state.released = false;
	}
	mouseDelta_ = glm::vec2{0.0F, 0.0F};
	scrollDelta_ = 0.0F;
}

void Input::onKey(int key, int action) {
	if (!inRange(key, kKeyCount) || action == GLFW_REPEAT) {
		return;
	}
	State& state = keys_[static_cast<std::size_t>(key)];
	if (action == GLFW_PRESS) {
		state.down = true;
		state.pressed = true;
	} else if (action == GLFW_RELEASE) {
		state.down = false;
		state.released = true;
	}
}

void Input::onMouseButton(int button, int action) {
	if (!inRange(button, kButtonCount)) {
		return;
	}
	State& state = buttons_[static_cast<std::size_t>(button)];
	if (action == GLFW_PRESS) {
		state.down = true;
		state.pressed = true;
	} else if (action == GLFW_RELEASE) {
		state.down = false;
		state.released = true;
	}
}

void Input::onCursorPos(double x, double y) {
	const glm::vec2 position{static_cast<float>(x), static_cast<float>(y)};

	// The first sample has no predecessor; reporting position - {0,0} would
	// produce one enormous delta and snap the camera on the first mouse move.
	if (hasCursorSample_) {
		mouseDelta_ += position - mousePosition_;
	} else {
		hasCursorSample_ = true;
	}
	mousePosition_ = position;
}

void Input::onScroll(double /*xOffset*/, double yOffset) {
	scrollDelta_ += static_cast<float>(yOffset);
}

bool Input::keyDown(int key) const {
	if (keyboardCaptured_ || !inRange(key, kKeyCount)) {
		return false;
	}
	return keys_[static_cast<std::size_t>(key)].down;
}

bool Input::keyPressed(int key) const {
	if (keyboardCaptured_ || !inRange(key, kKeyCount)) {
		return false;
	}
	return keys_[static_cast<std::size_t>(key)].pressed;
}

bool Input::keyReleased(int key) const {
	if (!inRange(key, kKeyCount)) {
		return false;
	}
	// Releases are reported even while captured, so a held key does not get
	// stuck down when a UI window takes focus mid-press.
	return keys_[static_cast<std::size_t>(key)].released;
}

bool Input::mouseDown(int button) const {
	if (mouseCaptured_ || !inRange(button, kButtonCount)) {
		return false;
	}
	return buttons_[static_cast<std::size_t>(button)].down;
}

bool Input::mousePressed(int button) const {
	if (mouseCaptured_ || !inRange(button, kButtonCount)) {
		return false;
	}
	return buttons_[static_cast<std::size_t>(button)].pressed;
}

bool Input::mouseReleased(int button) const {
	if (!inRange(button, kButtonCount)) {
		return false;
	}
	return buttons_[static_cast<std::size_t>(button)].released;
}

glm::vec2 Input::mouseDelta() const noexcept {
	return mouseCaptured_ ? glm::vec2{0.0F, 0.0F} : mouseDelta_;
}

float Input::scrollDelta() const noexcept {
	return mouseCaptured_ ? 0.0F : scrollDelta_;
}

} // namespace glc
