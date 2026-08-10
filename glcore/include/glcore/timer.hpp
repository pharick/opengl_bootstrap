#pragma once

namespace glc {

/// Frame timing.
///
/// tick() clamps the returned delta. humangl did not, so dragging or stalling
/// the window produced one enormous dt that teleported the simulation; a clamp
/// makes a stall look like a pause instead.
class FrameTimer {
public:
	FrameTimer();

	/// Seconds since the previous tick, clamped to maxDelta().
	float tick();

	/// Seconds since construction.
	[[nodiscard]] float elapsed() const;

	[[nodiscard]] float maxDelta() const noexcept {
		return maxDelta_;
	}
	void setMaxDelta(float seconds) noexcept {
		maxDelta_ = seconds;
	}

	/// Exponentially smoothed frame time, for display.
	[[nodiscard]] float smoothedFrameMs() const noexcept {
		return smoothedMs_;
	}
	[[nodiscard]] float smoothedFps() const noexcept {
		return smoothedMs_ > 0.0F ? 1000.0F / smoothedMs_ : 0.0F;
	}

	/// Total number of ticks so far.
	[[nodiscard]] unsigned long long frameCount() const noexcept {
		return frames_;
	}

private:
	double start_ = 0.0;
	double last_ = 0.0;
	float maxDelta_ = 0.1F;
	float smoothedMs_ = 16.6F;
	unsigned long long frames_ = 0;
};

} // namespace glc
