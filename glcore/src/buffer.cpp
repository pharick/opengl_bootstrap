#include <glcore/buffer.hpp>

#include <glcore/gl_check.hpp>

namespace glc {

void bufferData(const Buffer& buffer, GLenum target, const void* data, std::size_t bytes,
                GLenum usage) {
	glBindBuffer(target, buffer.id());
	GLC_CHECK(glBufferData(target, static_cast<GLsizeiptr>(bytes), data, usage));
	glBindBuffer(target, 0);
}

void bufferSubData(const Buffer& buffer, GLenum target, std::size_t offsetBytes, const void* data,
                   std::size_t bytes) {
	glBindBuffer(target, buffer.id());
	GLC_CHECK(glBufferSubData(target, static_cast<GLintptr>(offsetBytes),
	                          static_cast<GLsizeiptr>(bytes), data));
	glBindBuffer(target, 0);
}

} // namespace glc
