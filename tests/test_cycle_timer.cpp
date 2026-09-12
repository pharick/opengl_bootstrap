#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glcore/cycle_timer.hpp>

#include <stdexcept>

using Catch::Approx;

namespace {

constexpr float kDuration = 30.0F; // the sun's loop length in Tutorial 12

} // namespace

TEST_CASE("a new timer starts at the beginning of the cycle") {
	const glc::CycleTimer timer{kDuration};
	CHECK(timer.alpha() == Approx(0.0F));
	CHECK(timer.elapsed() == Approx(0.0F));
	CHECK(timer.duration() == Approx(kDuration));
	CHECK_FALSE(timer.isPaused());
}

TEST_CASE("alpha advances proportionally through the cycle") {
	glc::CycleTimer timer{kDuration};

	timer.update(kDuration / 4.0F);
	CHECK(timer.alpha() == Approx(0.25F));

	timer.update(kDuration / 4.0F);
	CHECK(timer.alpha() == Approx(0.5F));
	CHECK(timer.elapsed() == Approx(kDuration / 2.0F));
}

TEST_CASE("a delta spanning several cycles wraps rather than accumulating") {
	glc::CycleTimer timer{kDuration};
	timer.update(kDuration * 2.5F);
	CHECK(timer.alpha() == Approx(0.5F));
}

TEST_CASE("landing exactly on the duration reports the start, not the end") {
	glc::CycleTimer timer{kDuration};
	timer.update(kDuration);
	CHECK(timer.alpha() == Approx(0.0F));
}

// Regression test. Wrapping by repeated subtraction hangs here: once elapsed_
// passes ~1e9, subtracting 30.0F falls below one float ULP and the value stops
// changing, so the loop never exits. If this test hangs rather than fails,
// that is the bug.
TEST_CASE("a single enormous delta still leaves alpha in range") {
	glc::CycleTimer timer{kDuration};
	timer.update(1.0e9F);
	CHECK(timer.alpha() >= 0.0F);
	CHECK(timer.alpha() < 1.0F);
}

TEST_CASE("alpha stays in [0, 1) across a long run of deltas") {
	glc::CycleTimer timer{kDuration};
	for (int step = 0; step < 500; ++step) {
		timer.update(0.37F);
		REQUIRE(timer.alpha() >= 0.0F);
		REQUIRE(timer.alpha() < 1.0F);
	}
}

TEST_CASE("a paused timer ignores update") {
	glc::CycleTimer timer{kDuration};
	timer.update(kDuration / 4.0F);

	timer.setPaused(true);
	timer.update(kDuration / 2.0F);

	CHECK(timer.isPaused());
	CHECK(timer.alpha() == Approx(0.25F));

	timer.setPaused(false);
	timer.update(kDuration / 4.0F);
	CHECK(timer.alpha() == Approx(0.5F));
}

TEST_CASE("togglePause returns the state it switched to") {
	glc::CycleTimer timer{kDuration};
	CHECK(timer.togglePause());
	CHECK(timer.isPaused());
	CHECK_FALSE(timer.togglePause());
	CHECK_FALSE(timer.isPaused());
}

TEST_CASE("rewinding past zero wraps to the end of the cycle") {
	glc::CycleTimer timer{kDuration};
	timer.update(2.0F);
	timer.rewind(5.0F);
	CHECK(timer.elapsed() == Approx(kDuration - 3.0F));
	CHECK(timer.alpha() == Approx((kDuration - 3.0F) / kDuration));
}

TEST_CASE("rewinding several whole cycles still lands inside one") {
	glc::CycleTimer timer{kDuration};
	timer.update(kDuration / 2.0F);
	timer.rewind(kDuration * 3.0F);
	CHECK(timer.alpha() == Approx(0.5F));
}

TEST_CASE("fastForward wraps around the end") {
	glc::CycleTimer timer{kDuration};
	timer.update(kDuration - 2.0F);
	timer.fastForward(5.0F);
	CHECK(timer.elapsed() == Approx(3.0F));
}

TEST_CASE("scrubbing works while paused") {
	glc::CycleTimer timer{kDuration};
	timer.setPaused(true);

	timer.fastForward(kDuration / 2.0F);
	CHECK(timer.alpha() == Approx(0.5F));

	timer.rewind(kDuration / 4.0F);
	CHECK(timer.alpha() == Approx(0.25F));
}

TEST_CASE("reset returns to the start but leaves the pause state alone") {
	glc::CycleTimer timer{kDuration};
	timer.update(kDuration / 2.0F);
	timer.setPaused(true);

	timer.reset();

	CHECK(timer.alpha() == Approx(0.0F));
	CHECK(timer.isPaused());
}

TEST_CASE("setAlpha jumps to a position in the cycle") {
	glc::CycleTimer timer{kDuration};
	timer.setAlpha(0.25F);
	CHECK(timer.alpha() == Approx(0.25F));
	CHECK(timer.elapsed() == Approx(kDuration * 0.25F));
}

TEST_CASE("setAlpha wraps values outside [0, 1)") {
	glc::CycleTimer timer{kDuration};

	timer.setAlpha(1.25F);
	CHECK(timer.alpha() == Approx(0.25F));

	timer.setAlpha(-0.75F);
	CHECK(timer.alpha() == Approx(0.25F));
}

TEST_CASE("setAlpha works while paused") {
	glc::CycleTimer timer{kDuration};
	timer.setPaused(true);
	timer.setAlpha(0.6F);
	CHECK(timer.alpha() == Approx(0.6F));
	CHECK(timer.isPaused());
}

TEST_CASE("a non-positive duration is rejected at construction") {
	CHECK_THROWS_AS(glc::CycleTimer{0.0F}, std::invalid_argument);
	CHECK_THROWS_AS(glc::CycleTimer{-1.0F}, std::invalid_argument);
}
