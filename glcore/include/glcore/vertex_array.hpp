#pragma once

#include <glcore/handle.hpp>

#include <cstddef>
#include <span>

namespace glc {

/// Describes one vertex attribute. `location` must match the shader's
/// `layout(location = N)` -- keep the two together by defining the constant once
/// and using it in both places rather than repeating a bare literal.
struct AttributeDesc {
	GLuint location = 0;
	GLint components = 3; ///< 1..4
	GLenum type = GL_FLOAT;
	bool normalized = false; ///< integer types get scaled to [0,1] / [-1,1]
	GLsizei stride = 0;      ///< 0 means tightly packed
	std::size_t offset = 0;  ///< byte offset into the buffer
	bool integer = false;    ///< true -> glVertexAttribIPointer (no float conversion)
};

/// Creates a VAO capturing `attributes` sourced from `vbo`, plus an optional
/// element buffer. The element buffer binding is part of VAO state, so it stays
/// bound in the returned object.
[[nodiscard]] VertexArray makeVertexArray(const Buffer& vbo,
                                          std::span<const AttributeDesc> attributes,
                                          const Buffer* elementBuffer = nullptr);

/// Adds attributes sourced from a second buffer to an existing VAO, for
/// non-interleaved layouts where each attribute lives in its own buffer.
void addVertexAttributes(const VertexArray& vao, const Buffer& vbo,
                         std::span<const AttributeDesc> attributes);

} // namespace glc
