#include <doctest.h>

#include <glcore/matrix_stack.hpp>

#include <glm/gtc/epsilon.hpp>

namespace {

bool nearlyEqual(const glm::mat4& a, const glm::mat4& b, float epsilon = 1e-5F) {
	for (int column = 0; column < 4; ++column) {
		if (!glm::all(glm::epsilonEqual(a[column], b[column], epsilon))) {
			return false;
		}
	}
	return true;
}

} // namespace

TEST_CASE("a new stack holds one identity matrix") {
	const glc::MatrixStack stack;
	CHECK(stack.size() == 1);
	CHECK(nearlyEqual(stack.Top(), glm::mat4{1.0F}));
}

TEST_CASE("a frame pops at end of scope") {
	glc::MatrixStack stack;
	{
		const glc::MatrixStack::Frame frame = stack.push();
		CHECK(stack.size() == 2);
		stack.Translate(1.0F, 2.0F, 3.0F);
		CHECK_FALSE(nearlyEqual(stack.Top(), glm::mat4{1.0F}));
	}
	CHECK(stack.size() == 1);
	CHECK(nearlyEqual(stack.Top(), glm::mat4{1.0F}));
}

TEST_CASE("nested frames unwind in order") {
	glc::MatrixStack stack;
	{
		const glc::MatrixStack::Frame outer = stack.push();
		CHECK(outer.depth() == 2);
		{
			const glc::MatrixStack::Frame inner = stack.push();
			CHECK(inner.depth() == 3);
			CHECK(stack.size() == 3);
		}
		CHECK(stack.size() == 2);
	}
	CHECK(stack.size() == 1);
}

TEST_CASE("a frame is movable and only the owner pops") {
	glc::MatrixStack stack;
	{
		glc::MatrixStack::Frame frame = stack.push();
		CHECK(stack.size() == 2);

		// humangl's frame was non-movable, so it could not be returned or
		// stored. Moving must transfer the pop, not duplicate it.
		const glc::MatrixStack::Frame moved = std::move(frame);
		CHECK(stack.size() == 2);
	}
	CHECK(stack.size() == 1);
}

TEST_CASE("a child transform composes onto its parent") {
	glc::MatrixStack stack;
	stack.Translate(1.0F, 0.0F, 0.0F);
	{
		const glc::MatrixStack::Frame frame = stack.push();
		stack.Translate(2.0F, 0.0F, 0.0F);
		const glm::vec4 origin = stack.Top() * glm::vec4{0.0F, 0.0F, 0.0F, 1.0F};
		CHECK(origin.x == doctest::Approx(3.0F));
	}
	const glm::vec4 origin = stack.Top() * glm::vec4{0.0F, 0.0F, 0.0F, 1.0F};
	CHECK(origin.x == doctest::Approx(1.0F));
}

TEST_CASE("rotations are in degrees, matching the book") {
	glc::MatrixStack stack;
	stack.RotateZ(90.0F);
	const glm::vec4 rotated = stack.Top() * glm::vec4{1.0F, 0.0F, 0.0F, 1.0F};
	CHECK(rotated.x == doctest::Approx(0.0F).epsilon(1e-5));
	CHECK(rotated.y == doctest::Approx(1.0F));
}

TEST_CASE("operator*= applies a matrix to the frame") {
	glc::MatrixStack stack;
	glc::MatrixStack::Frame frame = stack.push();
	frame *= glm::mat4{1.0F};
	CHECK(nearlyEqual(frame.top(), glm::mat4{1.0F}));
}

TEST_CASE("popping the base matrix is refused rather than corrupting the stack") {
	glc::MatrixStack stack;
	stack.Pop(); // logs an error; must not empty the stack
	CHECK(stack.size() == 1);
}
