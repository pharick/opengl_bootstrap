#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <vector>

// A transform stack, deliberately API-compatible with gltut's glutil::MatrixStack.
//
// The PascalCase methods clash with the camelCase used everywhere else in
// glcore. That is intentional: code typed straight out of the book compiles
// unchanged, which is worth more here than naming consistency. Angles are in
// degrees, as in the book.
//
//     {
//         glc::MatrixStack::Frame frame = stack.push();
//         stack.Translate({1.0F, 0.0F, 0.0F});
//         stack.RotateY(30.0F);
//         program.set("modelToCamera", stack.Top());
//     }   // popped here
//
// Two fixes over humangl's version: the frame is movable, and the "innermost
// frame" invariant is checked in every build rather than through an assert that
// vanished under NDEBUG.

namespace glc {

class MatrixStack {
public:
	/// Scope guard returned by push(). Pops on destruction.
	class Frame {
	public:
		~Frame();

		Frame(Frame&& other) noexcept;
		Frame& operator=(Frame&& other) noexcept;
		Frame(const Frame&) = delete;
		Frame& operator=(const Frame&) = delete;

		[[nodiscard]] glm::mat4& top();
		[[nodiscard]] const glm::mat4& top() const;

		/// Right-multiplies the current matrix, like ApplyMatrix.
		Frame& operator*=(const glm::mat4& matrix);

		/// Nesting level of this frame, counting from 1.
		[[nodiscard]] std::size_t depth() const noexcept {
			return depth_;
		}

	private:
		friend class MatrixStack;
		Frame(MatrixStack& stack, std::size_t depth) noexcept : stack_{&stack}, depth_{depth} {}

		MatrixStack* stack_;
		std::size_t depth_;
	};

	MatrixStack();

	/// Duplicates the top matrix and returns a guard that pops it.
	[[nodiscard]] Frame push();

	// Manual pair, for transcribing book listings verbatim. Prefer push().
	void Push();
	/// noexcept so Frame's destructor and move assignment genuinely cannot throw.
	void Pop() noexcept;

	[[nodiscard]] const glm::mat4& Top() const;
	[[nodiscard]] glm::mat4& Top();

	/// Lowercase aliases, for consistency with the rest of glcore.
	[[nodiscard]] const glm::mat4& top() const {
		return Top();
	}
	[[nodiscard]] glm::mat4& top() {
		return Top();
	}

	void SetIdentity();
	void SetMatrix(const glm::mat4& matrix);
	void ApplyMatrix(const glm::mat4& matrix);

	void Translate(const glm::vec3& offset);
	void Translate(float x, float y, float z);

	void Scale(const glm::vec3& factors);
	void Scale(float x, float y, float z);
	void Scale(float uniform);

	void RotateX(float degrees);
	void RotateY(float degrees);
	void RotateZ(float degrees);
	void Rotate(const glm::vec3& axis, float degrees);

	void Perspective(float fovYDegrees, float aspect, float zNear, float zFar);
	void Orthographic(float left, float right, float bottom, float top, float zNear, float zFar);
	void LookAt(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& up);

	/// Number of matrices on the stack; always at least 1.
	[[nodiscard]] std::size_t size() const noexcept {
		return stack_.size();
	}

private:
	std::vector<glm::mat4> stack_;
};

} // namespace glc
