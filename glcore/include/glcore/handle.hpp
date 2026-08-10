#pragma once

#include <glcore/gl.hpp>

#include <utility>

// One move-only owner for every kind of GL object.
//
// Each GL resource type differs only in which pair of gl*/glDelete* functions it
// uses, so the ownership logic -- destructor, deleted copy, move that leaves the
// source empty -- is written once here instead of being hand-rolled per class.
//
// Lifetime note: a GlObject must be destroyed while its GL context is still
// current. Tutorials satisfy this for free, because members of a class derived
// from glc::App are destroyed before the App base subobject that owns the
// window and context.

namespace glc {

template<class Traits>
class GlObject {
public:
	GlObject() noexcept = default;

	/// Creates a new GL object. Only available for traits that can generate one
	/// without extra arguments (shaders need a stage, so they use adopt()).
	[[nodiscard]] static GlObject create()
	    requires requires { Traits::gen(); }
	{
		return GlObject{Traits::gen()};
	}

	/// Takes ownership of an already-created name.
	[[nodiscard]] static GlObject adopt(GLuint id) noexcept {
		return GlObject{id};
	}

	~GlObject() {
		reset();
	}

	GlObject(const GlObject&) = delete;
	GlObject& operator=(const GlObject&) = delete;

	GlObject(GlObject&& other) noexcept : id_{std::exchange(other.id_, 0)} {}

	GlObject& operator=(GlObject&& other) noexcept {
		if (this != &other) {
			reset();
			id_ = std::exchange(other.id_, 0);
		}
		return *this;
	}

	[[nodiscard]] GLuint id() const noexcept {
		return id_;
	}

	/// Deliberately explicit: humangl's implicit conversion let owning handles
	/// silently decay into raw names at call sites.
	[[nodiscard]] explicit operator bool() const noexcept {
		return id_ != 0;
	}

	void reset() noexcept {
		if (id_ != 0) {
			Traits::del(id_);
			id_ = 0;
		}
	}

	/// Relinquishes ownership without deleting.
	[[nodiscard]] GLuint release() noexcept {
		return std::exchange(id_, 0);
	}

private:
	explicit GlObject(GLuint id) noexcept : id_{id} {}

	GLuint id_ = 0;
};

namespace detail {

struct BufferTraits {
	static GLuint gen() {
		GLuint id = 0;
		glGenBuffers(1, &id);
		return id;
	}
	static void del(GLuint id) noexcept {
		glDeleteBuffers(1, &id);
	}
};

struct VertexArrayTraits {
	static GLuint gen() {
		GLuint id = 0;
		glGenVertexArrays(1, &id);
		return id;
	}
	static void del(GLuint id) noexcept {
		glDeleteVertexArrays(1, &id);
	}
};

struct TextureTraits {
	static GLuint gen() {
		GLuint id = 0;
		glGenTextures(1, &id);
		return id;
	}
	static void del(GLuint id) noexcept {
		glDeleteTextures(1, &id);
	}
};

struct SamplerTraits {
	static GLuint gen() {
		GLuint id = 0;
		glGenSamplers(1, &id);
		return id;
	}
	static void del(GLuint id) noexcept {
		glDeleteSamplers(1, &id);
	}
};

struct FramebufferTraits {
	static GLuint gen() {
		GLuint id = 0;
		glGenFramebuffers(1, &id);
		return id;
	}
	static void del(GLuint id) noexcept {
		glDeleteFramebuffers(1, &id);
	}
};

struct RenderbufferTraits {
	static GLuint gen() {
		GLuint id = 0;
		glGenRenderbuffers(1, &id);
		return id;
	}
	static void del(GLuint id) noexcept {
		glDeleteRenderbuffers(1, &id);
	}
};

struct ProgramTraits {
	static GLuint gen() {
		return glCreateProgram();
	}
	static void del(GLuint id) noexcept {
		glDeleteProgram(id);
	}
};

// No gen(): glCreateShader needs a stage, so shaders are created via
// ShaderHandle::adopt(glCreateShader(stage)).
struct ShaderTraits {
	static void del(GLuint id) noexcept {
		glDeleteShader(id);
	}
};

} // namespace detail

using Buffer = GlObject<detail::BufferTraits>;
using VertexArray = GlObject<detail::VertexArrayTraits>;
using Texture = GlObject<detail::TextureTraits>;
using Sampler = GlObject<detail::SamplerTraits>;
using Framebuffer = GlObject<detail::FramebufferTraits>;
using Renderbuffer = GlObject<detail::RenderbufferTraits>;
using ProgramHandle = GlObject<detail::ProgramTraits>;
using ShaderHandle = GlObject<detail::ShaderTraits>;

} // namespace glc
