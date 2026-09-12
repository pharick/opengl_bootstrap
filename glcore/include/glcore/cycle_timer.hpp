#pragma once

namespace glc {

/// A looping clock, driven by explicit deltas.
///
/// gltut's Framework::Timer reads an absolute clock inside Update(). This takes
/// the delta as an argument instead: App::onUpdate already supplies one with
/// AppConfig::maxDelta applied, and a timer that injects its own time cannot be
/// tested without sleeping.
///
/// Only the looping mode is here. The book also has single-shot and infinite
/// timers; Tutorial 12 uses neither.
class CycleTimer {
public:
	/// \throws std::invalid_argument if durationSeconds is not positive -- a
	/// zero or negative cycle has no meaningful alpha, and letting one through
	/// turns every later call into a division by zero.
	explicit CycleTimer(float durationSeconds);

	/// Advances the clock, unless paused. Any delta is accepted; the result is
	/// always wrapped back into the cycle.
	void update(float deltaSeconds);

	/// Progress through the cycle, in [0, 1).
	[[nodiscard]] float alpha() const;

	/// Jumps to a position in the cycle. Values outside [0, 1) wrap, so 1.25
	/// and -0.75 both land at 0.25. Works while paused -- this is what a scrub
	/// widget drives.
	void setAlpha(float alpha);

	/// Position within the current cycle, in [0, duration). Resets every loop;
	/// this is not the total time since construction.
	[[nodiscard]] float elapsed() const;

	[[nodiscard]] float duration() const;

	void setPaused(bool paused);
	/// Flips the pause state and returns the new one.
	bool togglePause();
	[[nodiscard]] bool isPaused() const;

	// Scrubbing. Deliberately works while paused -- that is the point of it.
	void rewind(float seconds);
	void fastForward(float seconds);

	void reset();

private:
	/// Normalizes elapsed_ back into [0, duration_).
	void wrap();

	float duration_;
	float elapsed_{0.0F};
	bool paused_{false};
};

} // namespace glc
