#include <glcore/vertex_array.hpp>

#include <glcore/gl_check.hpp>

#include <cstdint>

namespace glc {
namespace {

/// glVertexAttribPointer takes its buffer offset as a pointer-typed integer.
/// The int-to-pointer cast is imposed by the GL API, not a choice.
const void* byteOffset(std::size_t offset) {
	// NOLINTNEXTLINE(performance-no-int-to-ptr)
	return reinterpret_cast<const void*>(static_cast<std::uintptr_t>(offset));
}

void applyAttributes(std::span<const AttributeDesc> attributes) {
	for (const AttributeDesc& attribute : attributes) {
		glEnableVertexAttribArray(attribute.location);
		if (attribute.integer) {
			GLC_CHECK(glVertexAttribIPointer(attribute.location, attribute.components,
			                                 attribute.type, attribute.stride,
			                                 byteOffset(attribute.offset)));
		} else {
			GLC_CHECK(glVertexAttribPointer(attribute.location, attribute.components,
			                                attribute.type,
			                                attribute.normalized ? GL_TRUE : GL_FALSE,
			                                attribute.stride, byteOffset(attribute.offset)));
		}
	}
}

} // namespace

VertexArray makeVertexArray(const Buffer& vbo, std::span<const AttributeDesc> attributes,
                            const Buffer* elementBuffer) {
	VertexArray vao = VertexArray::create();

	glBindVertexArray(vao.id());
	glBindBuffer(GL_ARRAY_BUFFER, vbo.id());
	applyAttributes(attributes);

	if (elementBuffer != nullptr) {
		// GL_ELEMENT_ARRAY_BUFFER binding is captured by the VAO, so it must be
		// bound here and must NOT be unbound before the VAO is unbound.
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer->id());
	}

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	return vao;
}

void addVertexAttributes(const VertexArray& vao, const Buffer& vbo,
                         std::span<const AttributeDesc> attributes) {
	glBindVertexArray(vao.id());
	glBindBuffer(GL_ARRAY_BUFFER, vbo.id());
	applyAttributes(attributes);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

} // namespace glc
