#include <glcore/matrix_stack.hpp>

#include <glcore/log.hpp>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

#include <utility>

namespace glc {
namespace {

/// Enough for a deep scene graph without reallocating mid-frame.
constexpr std::size_t kInitialCapacity = 32;

} // namespace

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------

MatrixStack::Frame::Frame(Frame&& other) noexcept
    : stack_{std::exchange(other.stack_, nullptr)}, depth_{other.depth_} {}

MatrixStack::Frame& MatrixStack::Frame::operator=(Frame&& other) noexcept {
	if (this != &other) {
		if (stack_ != nullptr) {
			stack_->Pop();
		}
		stack_ = std::exchange(other.stack_, nullptr);
		depth_ = other.depth_;
	}
	return *this;
}

MatrixStack::Frame::~Frame() {
	if (stack_ == nullptr) {
		return; // moved from
	}

	// Checked in every build. humangl used assert here, which meant the
	// invariant was not enforced in its default RelWithDebInfo build.
	if (stack_->size() != depth_) {
		log::error("matrix stack frame at depth {} destroyed while the stack is at depth {} -- "
		           "an inner frame outlived its parent; unwinding to keep the stack consistent",
		           depth_, stack_->size());
		while (stack_->size() >= depth_ && stack_->size() > 1) {
			stack_->Pop();
		}
		stack_ = nullptr;
		return;
	}

	stack_->Pop();
	stack_ = nullptr;
}

glm::mat4& MatrixStack::Frame::top() {
	return stack_->Top();
}

const glm::mat4& MatrixStack::Frame::top() const {
	return stack_->Top();
}

MatrixStack::Frame& MatrixStack::Frame::operator*=(const glm::mat4& matrix) {
	stack_->ApplyMatrix(matrix);
	return *this;
}

// ---------------------------------------------------------------------------
// MatrixStack
// ---------------------------------------------------------------------------

MatrixStack::MatrixStack() {
	stack_.reserve(kInitialCapacity);
	stack_.emplace_back(1.0F);
}

MatrixStack::Frame MatrixStack::push() {
	Push();
	return Frame{*this, stack_.size()};
}

void MatrixStack::Push() {
	stack_.push_back(stack_.back());
}

void MatrixStack::Pop() noexcept {
	if (stack_.size() <= 1) {
		log::error("matrix stack underflow: Pop() on the base matrix is ignored");
		return;
	}
	stack_.pop_back();
}

const glm::mat4& MatrixStack::Top() const {
	return stack_.back();
}

glm::mat4& MatrixStack::Top() {
	return stack_.back();
}

void MatrixStack::SetIdentity() {
	stack_.back() = glm::mat4{1.0F};
}

void MatrixStack::SetMatrix(const glm::mat4& matrix) {
	stack_.back() = matrix;
}

void MatrixStack::ApplyMatrix(const glm::mat4& matrix) {
	stack_.back() *= matrix;
}

void MatrixStack::Translate(const glm::vec3& offset) {
	stack_.back() = glm::translate(stack_.back(), offset);
}

void MatrixStack::Translate(float x, float y, float z) {
	Translate(glm::vec3{x, y, z});
}

void MatrixStack::Scale(const glm::vec3& factors) {
	stack_.back() = glm::scale(stack_.back(), factors);
}

void MatrixStack::Scale(float x, float y, float z) {
	Scale(glm::vec3{x, y, z});
}

void MatrixStack::Scale(float uniform) {
	Scale(glm::vec3{uniform, uniform, uniform});
}

void MatrixStack::RotateX(float degrees) {
	Rotate(glm::vec3{1.0F, 0.0F, 0.0F}, degrees);
}

void MatrixStack::RotateY(float degrees) {
	Rotate(glm::vec3{0.0F, 1.0F, 0.0F}, degrees);
}

void MatrixStack::RotateZ(float degrees) {
	Rotate(glm::vec3{0.0F, 0.0F, 1.0F}, degrees);
}

void MatrixStack::Rotate(const glm::vec3& axis, float degrees) {
	stack_.back() = glm::rotate(stack_.back(), glm::radians(degrees), axis);
}

void MatrixStack::Perspective(float fovYDegrees, float aspect, float zNear, float zFar) {
	stack_.back() = glm::perspective(glm::radians(fovYDegrees), aspect, zNear, zFar);
}

void MatrixStack::Orthographic(float left, float right, float bottom, float top, float zNear,
                               float zFar) {
	stack_.back() = glm::ortho(left, right, bottom, top, zNear, zFar);
}

void MatrixStack::LookAt(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& up) {
	stack_.back() = glm::lookAt(eye, target, up);
}

} // namespace glc
