#pragma once

#include <glcore/handle.hpp>

#include <cstddef>
#include <span>

// Uniform buffer objects, used from Tutorial 9 (Lights On) onward.
//
// A UBO carries a block of uniforms shared by several programs. Bind the buffer
// to a numbered binding point, bind each program's matching uniform block to
// the same point with Program::bindUniformBlock, and one upload feeds them all.
//
//     constexpr GLuint kGlobalMatrices = 0;
//     auto ubo = glc::UniformBuffer::create(sizeof(GlobalMatrices));
//     ubo.bindToPoint(kGlobalMatrices);
//     program.bindUniformBlock("GlobalMatrices", kGlobalMatrices);
//     ubo.update(matrices);
//
// Remember that std140 layout rules apply to the struct you upload: vec3 is
// padded to 16 bytes, and array elements are rounded up to vec4. Declare the
// block `layout(std140)` in GLSL and mirror the padding in C++.

namespace glc {

class UniformBuffer {
public:
	[[nodiscard]] static UniformBuffer create(std::size_t bytes, GLenum usage = GL_DYNAMIC_DRAW);

	/// Allocates a buffer sized for T.
	template<class T>
	[[nodiscard]] static UniformBuffer forType(GLenum usage = GL_DYNAMIC_DRAW) {
		return create(sizeof(T), usage);
	}

	void update(const void* data, std::size_t bytes, std::size_t offsetBytes = 0) const;

	template<class T>
	void update(const T& value, std::size_t offsetBytes = 0) const {
		update(&value, sizeof(T), offsetBytes);
	}

	/// Binds the whole buffer to a binding point.
	void bindToPoint(GLuint bindingPoint) const;

	/// Binds a sub-range. `offsetBytes` must be a multiple of
	/// offsetAlignment().
	void bindRange(GLuint bindingPoint, std::size_t offsetBytes, std::size_t bytes) const;

	[[nodiscard]] const Buffer& buffer() const noexcept {
		return buffer_;
	}
	[[nodiscard]] std::size_t size() const noexcept {
		return size_;
	}

	/// GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT for this driver. Required when
	/// packing several blocks into one buffer.
	[[nodiscard]] static GLint offsetAlignment();

private:
	UniformBuffer(Buffer buffer, std::size_t size) noexcept;

	Buffer buffer_;
	std::size_t size_ = 0;
};

} // namespace glc
