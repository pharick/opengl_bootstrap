#pragma once

#include <glcore/handle.hpp>

#include <cstdint>

// OpenGL 3.3 has no direct state access (glCreateBuffers / glNamedBufferStorage
// arrived in 4.5), so binding is unavoidable. Rather than leaving bind/unbind
// pairs to be matched by hand, tie them to a scope.
//
//     {
//         glc::ScopedBind bind{vao};
//         glDrawArrays(GL_TRIANGLES, 0, 3);
//     }   // VAO unbound here
//
// Unbinding restores 0 rather than the previously bound object: querying the
// previous binding costs a glGet round-trip, and returning to 0 matches how the
// gltut sample code is written.

namespace glc {

class ScopedBind {
public:
	explicit ScopedBind(const VertexArray& vao) noexcept;
	ScopedBind(GLenum target, const Buffer& buffer) noexcept;
	ScopedBind(GLenum target, const Texture& texture, GLuint unit = 0) noexcept;

	~ScopedBind();

	ScopedBind(const ScopedBind&) = delete;
	ScopedBind& operator=(const ScopedBind&) = delete;
	ScopedBind(ScopedBind&&) = delete;
	ScopedBind& operator=(ScopedBind&&) = delete;

private:
	enum class Kind : std::uint8_t { Vao, Buffer, Texture };

	Kind kind_;
	GLenum target_ = 0;
	GLuint unit_ = 0;
};

} // namespace glc
