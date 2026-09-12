#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glcore/interpolator.hpp>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <vector>

using Catch::Approx;

namespace {

using Vec3Path = glc::ConstVelLinearInterpolator<glm::vec3>;

bool nearlyEqual(const glm::vec3& a, const glm::vec3& b, float epsilon = 1e-4F) {
	return glm::distance(a, b) <= epsilon;
}

} // namespace

// ---------------------------------------------------------------------------
// TimedLinearInterpolator
// ---------------------------------------------------------------------------

TEST_CASE("an empty timed interpolator yields a value-initialized result") {
	const glc::TimedLinearInterpolator<float> interpolator;
	CHECK(interpolator.empty());
	CHECK(interpolator.segmentCount() == 0);
	CHECK(interpolator.interpolate(0.5F) == Approx(0.0F));
}

TEST_CASE("a single keyframe holds its value everywhere") {
	glc::TimedLinearInterpolator<float> interpolator;
	const std::array keyframes{glc::Keyframe<float>{.time = 0.3F, .value = 7.0F}};
	interpolator.setKeyframes(keyframes, false);

	CHECK(interpolator.interpolate(0.0F) == Approx(7.0F));
	CHECK(interpolator.interpolate(0.3F) == Approx(7.0F));
	CHECK(interpolator.interpolate(1.0F) == Approx(7.0F));
}

TEST_CASE("a non-looping interpolator lerps between its endpoints") {
	glc::TimedLinearInterpolator<float> interpolator;
	const std::array keyframes{
	    glc::Keyframe<float>{.time = 0.0F, .value = 0.0F},
	    glc::Keyframe<float>{.time = 1.0F, .value = 10.0F},
	};
	interpolator.setKeyframes(keyframes, false);

	CHECK(interpolator.interpolate(0.0F) == Approx(0.0F));
	CHECK(interpolator.interpolate(0.25F) == Approx(2.5F));
	CHECK(interpolator.interpolate(1.0F) == Approx(10.0F));
}

TEST_CASE("alpha outside [0, 1] clamps instead of extrapolating") {
	glc::TimedLinearInterpolator<float> interpolator;
	const std::array keyframes{
	    glc::Keyframe<float>{.time = 0.0F, .value = 2.0F},
	    glc::Keyframe<float>{.time = 1.0F, .value = 4.0F},
	};
	interpolator.setKeyframes(keyframes, false);

	CHECK(interpolator.interpolate(-5.0F) == Approx(2.0F));
	CHECK(interpolator.interpolate(37.0F) == Approx(4.0F));
}

TEST_CASE("unevenly spaced keyframes lerp within the segment they belong to") {
	glc::TimedLinearInterpolator<float> interpolator;
	const std::array keyframes{
	    glc::Keyframe<float>{.time = 0.0F, .value = 0.0F},
	    glc::Keyframe<float>{.time = 0.8F, .value = 100.0F},
	    glc::Keyframe<float>{.time = 1.0F, .value = 200.0F},
	};
	interpolator.setKeyframes(keyframes, false);

	// Halfway through the long first segment, not halfway through the range.
	CHECK(interpolator.interpolate(0.4F) == Approx(50.0F));
	// Halfway through the short second segment.
	CHECK(interpolator.interpolate(0.9F) == Approx(150.0F));
}

TEST_CASE("looping appends a return segment back to the first keyframe") {
	glc::TimedLinearInterpolator<float> interpolator;
	const std::array keyframes{
	    glc::Keyframe<float>{.time = 0.0F, .value = 0.0F},
	    glc::Keyframe<float>{.time = 0.5F, .value = 10.0F},
	};
	interpolator.setKeyframes(keyframes, true);

	CHECK(interpolator.segmentCount() == 2);
	CHECK(interpolator.interpolate(0.5F) == Approx(10.0F));
	// The extra segment runs 0.5 -> 1.0, travelling from 10 back to 0.
	CHECK(interpolator.interpolate(0.75F) == Approx(5.0F));
	CHECK(interpolator.interpolate(1.0F) == Approx(0.0F));
}

TEST_CASE("the first and last samples are pinned to 0 and 1") {
	glc::TimedLinearInterpolator<float> interpolator;
	// Authored times cover neither end of the range.
	const std::array keyframes{
	    glc::Keyframe<float>{.time = 0.2F, .value = 1.0F},
	    glc::Keyframe<float>{.time = 0.6F, .value = 3.0F},
	};
	interpolator.setKeyframes(keyframes, false);

	CHECK(interpolator.interpolate(0.0F) == Approx(1.0F));
	CHECK(interpolator.interpolate(0.5F) == Approx(2.0F));
	CHECK(interpolator.interpolate(1.0F) == Approx(3.0F));
}

TEST_CASE("a timed interpolator works on vectors, not just scalars") {
	glc::TimedLinearInterpolator<glm::vec3> interpolator;
	const std::array keyframes{
	    glc::Keyframe<glm::vec3>{.time = 0.0F, .value = glm::vec3{0.0F}},
	    glc::Keyframe<glm::vec3>{.time = 1.0F, .value = glm::vec3{2.0F, 4.0F, 6.0F}},
	};
	interpolator.setKeyframes(keyframes, false);

	CHECK(nearlyEqual(interpolator.interpolate(0.5F), glm::vec3{1.0F, 2.0F, 3.0F}));
}

// ---------------------------------------------------------------------------
// ConstVelLinearInterpolator
// ---------------------------------------------------------------------------

TEST_CASE("path length is the sum of the segments") {
	Vec3Path path;
	const std::array waypoints{
	    glm::vec3{0.0F, 0.0F, 0.0F},
	    glm::vec3{3.0F, 0.0F, 0.0F},
	    glm::vec3{3.0F, 4.0F, 0.0F},
	};
	path.setWaypoints(waypoints, false);

	CHECK(path.totalDistance() == Approx(7.0F)); // 3 + 4
	CHECK(path.segmentCount() == 2);
}

TEST_CASE("a looping path measures the leg back to the start") {
	Vec3Path path;
	const std::array waypoints{
	    glm::vec3{0.0F, 0.0F, 0.0F},
	    glm::vec3{3.0F, 0.0F, 0.0F},
	    glm::vec3{3.0F, 4.0F, 0.0F},
	};
	path.setWaypoints(waypoints, true);

	CHECK(path.totalDistance() == Approx(12.0F)); // 3 + 4 + 5 (the 3-4-5 hypotenuse)
	CHECK(path.segmentCount() == 3);
	CHECK(nearlyEqual(path.interpolate(1.0F), waypoints[0]));
}

// The reason this class exists. With weights taken from the waypoint index, a
// half-alpha step would land at the midpoint of the *list* -- here, one unit
// along a path eleven units long.
TEST_CASE("alpha maps to distance travelled, not to waypoint index") {
	Vec3Path path;
	const std::array waypoints{
	    glm::vec3{0.0F, 0.0F, 0.0F},
	    glm::vec3{1.0F, 0.0F, 0.0F},  // 1 unit in
	    glm::vec3{11.0F, 0.0F, 0.0F}, // 11 units in
	};
	path.setWaypoints(waypoints, false);

	CHECK(path.totalDistance() == Approx(11.0F));
	CHECK(nearlyEqual(path.interpolate(0.5F), glm::vec3{5.5F, 0.0F, 0.0F}));
	CHECK(nearlyEqual(path.interpolate(0.25F), glm::vec3{2.75F, 0.0F, 0.0F}));
}

TEST_CASE("equal alpha steps cover equal distance on an uneven path") {
	// Collinear on purpose. A step that straddles a corner cuts a chord rather
	// than following the path, so a turn would blur the very property under
	// test; segment *spacing* is what varies here, from 1 unit to 20.
	Vec3Path path;
	const std::array waypoints{
	    glm::vec3{0.0F, 0.0F, 0.0F},
	    glm::vec3{1.0F, 0.0F, 0.0F},  // a very short segment
	    glm::vec3{21.0F, 0.0F, 0.0F}, // a very long one
	    glm::vec3{24.0F, 0.0F, 0.0F}, // and a short one again
	};
	path.setWaypoints(waypoints, false);

	constexpr int kSteps = 20;
	std::vector<float> stepDistances;
	stepDistances.reserve(kSteps);

	glm::vec3 previous = path.interpolate(0.0F);
	for (int step = 1; step <= kSteps; ++step) {
		const glm::vec3 current = path.interpolate(static_cast<float>(step) / kSteps);
		stepDistances.push_back(glm::distance(previous, current));
		previous = current;
	}

	const float expected = path.totalDistance() / kSteps;
	for (const float travelled : stepDistances) {
		// Index-based weighting would make these range from 0.2 to 4.0.
		CHECK(travelled == Approx(expected).margin(1e-4F));
	}
}

TEST_CASE("index-based spacing would not have constant velocity") {
	// Same path expressed as evenly-timed keyframes: the contrast case for the
	// test above. Alpha 0.5 lands at the midpoint of the list, one unit along.
	glc::TimedLinearInterpolator<glm::vec3> byIndex;
	const std::array keyframes{
	    glc::Keyframe<glm::vec3>{.time = 0.0F, .value = glm::vec3{0.0F, 0.0F, 0.0F}},
	    glc::Keyframe<glm::vec3>{.time = 0.5F, .value = glm::vec3{1.0F, 0.0F, 0.0F}},
	    glc::Keyframe<glm::vec3>{.time = 1.0F, .value = glm::vec3{11.0F, 0.0F, 0.0F}},
	};
	byIndex.setKeyframes(keyframes, false);

	CHECK(nearlyEqual(byIndex.interpolate(0.5F), glm::vec3{1.0F, 0.0F, 0.0F}));

	Vec3Path byDistance;
	const std::array waypoints{
	    glm::vec3{0.0F, 0.0F, 0.0F},
	    glm::vec3{1.0F, 0.0F, 0.0F},
	    glm::vec3{11.0F, 0.0F, 0.0F},
	};
	byDistance.setWaypoints(waypoints, false);

	CHECK(nearlyEqual(byDistance.interpolate(0.5F), glm::vec3{5.5F, 0.0F, 0.0F}));
}

TEST_CASE("a degenerate path of coincident waypoints does not divide by zero") {
	Vec3Path path;
	const std::array waypoints{glm::vec3{2.0F, 2.0F, 2.0F}, glm::vec3{2.0F, 2.0F, 2.0F}};
	path.setWaypoints(waypoints, false);

	CHECK(path.totalDistance() == Approx(0.0F));
	CHECK(nearlyEqual(path.interpolate(0.5F), glm::vec3{2.0F, 2.0F, 2.0F}));
}

TEST_CASE("an empty path yields a value-initialized result") {
	const Vec3Path path;
	CHECK(path.empty());
	CHECK(nearlyEqual(path.interpolate(0.5F), glm::vec3{0.0F}));
}
