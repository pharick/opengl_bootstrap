#pragma once

#include <glm/geometric.hpp>

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

// Piecewise-linear interpolation over a list of samples, each carrying a weight
// in [0, 1]. Two flavours, differing only in where the weights come from:
//
//   TimedLinearInterpolator     weights are authored -- "at 6:30, the sun is
//                               this colour". Uneven spacing is the point.
//
//   ConstVelLinearInterpolator  weights are derived from arc length, so equal
//                               steps in alpha cover equal distance regardless
//                               of how the waypoints are spaced.
//
// gltut's versions (framework/Interpolators.h) find the value and weight of a
// sample through free functions located by ADL, which means adding a new
// element type means adding overloads in a header that knows nothing about it.
// Here the two are separate types instead: Keyframe carries its own time, and
// the constant-velocity one takes bare waypoints because it computes the
// weights itself.

namespace glc {

namespace detail {

template<class T>
struct WeightedSample {
	T value;
	float weight;
};

/// Finds the segment containing `alpha` and lerps across it. Clamps to the end
/// values outside the sampled range rather than extrapolating.
template<class T>
[[nodiscard]] T interpolateWeighted(const std::vector<WeightedSample<T>>& samples, float alpha) {
	if (samples.empty()) {
		return T{};
	}
	if (samples.size() == 1 || alpha <= samples.front().weight) {
		return samples.front().value;
	}
	if (alpha >= samples.back().weight) {
		return samples.back().value;
	}

	// alpha is strictly inside the range, so upper is neither begin() nor end().
	const auto upper = std::ranges::upper_bound(samples, alpha, {}, &WeightedSample<T>::weight);
	const auto lower = std::prev(upper);

	const float span = upper->weight - lower->weight;
	if (span <= 0.0F) {
		return lower->value; // duplicate weights: no segment to travel along
	}

	const float t = (alpha - lower->weight) / span;
	return (lower->value * (1.0F - t)) + (upper->value * t);
}

} // namespace detail

/// A value and the normalized time at which it applies.
template<class T>
struct Keyframe {
	float time; ///< in [0, 1]
	T value;
};

/// Interpolates between keyframes placed at authored times.
///
/// Tutorial 12 drives ambient light, sunlight intensity, background colour and
/// (from the HDR stage) maxIntensity from one of these, all sharing the sun
/// timer's alpha.
template<class T>
class TimedLinearInterpolator {
public:
	/// When `looping`, a copy of the first keyframe is appended so the last
	/// segment travels back to the start. Either way the first sample is pinned
	/// to weight 0 and the last to weight 1, so the full [0, 1] range is
	/// covered no matter what times were authored.
	void setKeyframes(std::span<const Keyframe<T>> keyframes, bool looping = true) {
		samples_.clear();
		samples_.reserve(keyframes.size() + (looping ? 1 : 0));
		for (const Keyframe<T>& keyframe : keyframes) {
			samples_.push_back({.value = keyframe.value, .weight = keyframe.time});
		}
		if (samples_.empty()) {
			return;
		}
		if (looping) {
			samples_.push_back(samples_.front());
		}
		samples_.front().weight = 0.0F;
		samples_.back().weight = 1.0F;
	}

	[[nodiscard]] T interpolate(float alpha) const {
		return detail::interpolateWeighted(samples_, alpha);
	}

	[[nodiscard]] bool empty() const noexcept {
		return samples_.empty();
	}
	[[nodiscard]] std::size_t segmentCount() const noexcept {
		return samples_.empty() ? 0 : samples_.size() - 1;
	}

private:
	std::vector<detail::WeightedSample<T>> samples_;
};

/// Interpolates along a path at a constant speed.
///
/// Weights come from cumulative distance rather than index, so adding 0.1 to
/// alpha always advances the same distance along the path. Feed it unevenly
/// spaced waypoints and the motion still looks steady -- which is what the
/// point lights in Tutorial 12 need, since their paths are hand-authored and
/// nowhere near uniform.
///
/// `T` must be a glm vector type; the weights are built with glm::distance.
template<class T>
class ConstVelLinearInterpolator {
public:
	/// When `looping`, a copy of the first waypoint is appended, so the path
	/// closes and the return leg is measured like any other segment.
	void setWaypoints(std::span<const T> waypoints, bool looping = true) {
		samples_.clear();
		totalDistance_ = 0.0F;
		samples_.reserve(waypoints.size() + (looping ? 1 : 0));
		for (const T& waypoint : waypoints) {
			samples_.push_back({.value = waypoint, .weight = 0.0F});
		}
		if (samples_.empty()) {
			return;
		}
		if (looping) {
			samples_.push_back(samples_.front());
		}

		for (std::size_t i = 1; i < samples_.size(); ++i) {
			totalDistance_ += glm::distance(samples_[i - 1].value, samples_[i].value);
			samples_[i].weight = totalDistance_;
		}
		if (totalDistance_ <= 0.0F) {
			return; // every waypoint coincides; leave the weights at zero
		}
		for (std::size_t i = 1; i < samples_.size(); ++i) {
			samples_[i].weight /= totalDistance_;
		}
	}

	[[nodiscard]] T interpolate(float alpha) const {
		return detail::interpolateWeighted(samples_, alpha);
	}

	/// Length of the whole path, in world units.
	[[nodiscard]] float totalDistance() const noexcept {
		return totalDistance_;
	}

	[[nodiscard]] bool empty() const noexcept {
		return samples_.empty();
	}
	[[nodiscard]] std::size_t segmentCount() const noexcept {
		return samples_.empty() ? 0 : samples_.size() - 1;
	}

private:
	std::vector<detail::WeightedSample<T>> samples_;
	float totalDistance_{0.0F};
};

} // namespace glc
