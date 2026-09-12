#include <glcore/cycle_timer.hpp>

#include <cmath>
#include <format>
#include <stdexcept>

namespace glc {

CycleTimer::CycleTimer(float durationSeconds) : duration_{durationSeconds} {
	if (!(durationSeconds > 0.0F)) { // also rejects NaN
		throw std::invalid_argument(
		    std::format("CycleTimer duration must be positive, got {}", durationSeconds));
	}
}

// Repeated subtraction is the obvious way to wrap and it is wrong: once
// elapsed_ grows past ~1e9, subtracting a 30-second duration falls below one
// float ULP and leaves the value bit-identical, so the loop never terminates.
// fmod is O(1) and has no such cliff.
void CycleTimer::wrap() {
	elapsed_ = std::fmod(elapsed_, duration_);

	// fmod keeps the sign of its first argument, so a rewind past zero lands on
	// a negative remainder rather than the end of the cycle.
	if (elapsed_ < 0.0F) {
		elapsed_ += duration_;

		// A remainder small enough that adding it to duration_ rounds back up
		// to duration_ would otherwise report alpha() == 1.0.
		if (elapsed_ >= duration_) {
			elapsed_ = 0.0F;
		}
	}
}

void CycleTimer::update(float deltaSeconds) {
	if (paused_) {
		return;
	}
	elapsed_ += deltaSeconds;
	wrap();
}

float CycleTimer::alpha() const {
	return elapsed_ / duration_;
}

void CycleTimer::setAlpha(float alpha) {
	elapsed_ = alpha * duration_;
	wrap();
}

float CycleTimer::elapsed() const {
	return elapsed_;
}

float CycleTimer::duration() const {
	return duration_;
}

void CycleTimer::setPaused(bool paused) {
	paused_ = paused;
}

bool CycleTimer::togglePause() {
	paused_ = !paused_;
	return paused_;
}

bool CycleTimer::isPaused() const {
	return paused_;
}

void CycleTimer::rewind(float seconds) {
	fastForward(-seconds);
}

void CycleTimer::fastForward(float seconds) {
	elapsed_ += seconds;
	wrap();
}

void CycleTimer::reset() {
	elapsed_ = 0.0F;
}

} // namespace glc
