#include <glcore/uniform_buffer.hpp>

#include <glcore/buffer.hpp>
#include <glcore/gl_check.hpp>

#include <format>
#include <stdexcept>
#include <utility>

namespace glc {

UniformBuffer::UniformBuffer(Buffer buffer, std::size_t size) noexcept
    : buffer_{std::move(buffer)}, size_{size} {}

UniformBuffer UniformBuffer::create(std::size_t bytes, GLenum usage) {
	Buffer buffer = Buffer::create();
	bufferData(buffer, GL_UNIFORM_BUFFER, nullptr, bytes, usage);
	return UniformBuffer{std::move(buffer), bytes};
}

void UniformBuffer::update(const void* data, std::size_t bytes, std::size_t offsetBytes) const {
	if (offsetBytes + bytes > size_) {
		throw std::out_of_range(
		    std::format("uniform buffer update of {} bytes at offset {} exceeds its size of {}",
			            bytes, offsetBytes, size_));
	}
	bufferSubData(buffer_, GL_UNIFORM_BUFFER, offsetBytes, data, bytes);
}

void UniformBuffer::bindToPoint(GLuint bindingPoint) const {
	GLC_CHECK(glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, buffer_.id()));
}

void UniformBuffer::bindRange(GLuint bindingPoint, std::size_t offsetBytes,
                              std::size_t bytes) const {
	GLC_CHECK(glBindBufferRange(GL_UNIFORM_BUFFER, bindingPoint, buffer_.id(),
	                            static_cast<GLintptr>(offsetBytes),
	                            static_cast<GLsizeiptr>(bytes)));
}

GLint UniformBuffer::offsetAlignment() {
	GLint alignment = 0;
	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);
	return alignment;
}

} // namespace glc
