#include <glcore/frame_timer.hpp>

#include <GLFW/glfw3.h>

#include <algorithm>

namespace glc {
namespace {

/// Weight of the newest sample in the smoothed frame time.
constexpr float kSmoothing = 0.1F;

} // namespace

FrameTimer::FrameTimer() : start_{glfwGetTime()}, last_{start_} {}

float FrameTimer::tick() {
	const double now = glfwGetTime();
	const auto raw = static_cast<float>(now - last_);
	last_ = now;
	++frames_;

	smoothedMs_ += ((raw * 1000.0F) - smoothedMs_) * kSmoothing;

	return std::min(raw, maxDelta_);
}

float FrameTimer::elapsed() const {
	return static_cast<float>(glfwGetTime() - start_);
}

} // namespace glc
