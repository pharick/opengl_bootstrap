#pragma once

#include <glcore/handle.hpp>

#include <glm/glm.hpp>

#include <expected>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace glc {

/// One stage of a program: which shader type, and where its source lives.
struct ShaderSource {
	GLenum stage;
	std::filesystem::path path;
};

/// A linked shader program.
///
/// There is no default constructor and no isValid(): a Program either exists and
/// is usable, or construction failed. Use tryFrom*() when a failure is expected
/// and recoverable (hot-reload), and from*() when it is fatal (startup).
class Program {
public:
	[[nodiscard]] static Program fromFiles(const std::filesystem::path& vertex,
	                                       const std::filesystem::path& fragment);
	[[nodiscard]] static Program fromFiles(std::span<const ShaderSource> sources);
	[[nodiscard]] static std::expected<Program, std::string>
	tryFromFiles(std::span<const ShaderSource> sources);

	void use() const;
	static void unuse();

	[[nodiscard]] GLuint id() const noexcept {
		return handle_.id();
	}

	/// Cached lookup. Returns -1 for a name the linker optimized out.
	[[nodiscard]] GLint uniformLocation(std::string_view name) const;

	/// Cached lookup that throws if the uniform is absent -- catches typos at
	/// startup rather than silently drawing nothing.
	[[nodiscard]] GLint requireUniform(std::string_view name) const;

	/// Binds a uniform block to a binding point (Tutorial 9+).
	void bindUniformBlock(std::string_view blockName, GLuint bindingPoint) const;

	// GL 3.3 has no glProgramUniform, so these act on the *currently bound*
	// program. Debug builds verify that this program is the bound one.
	void set(GLint location, bool value) const;
	void set(GLint location, int value) const;
	void set(GLint location, unsigned int value) const;
	void set(GLint location, float value) const;
	void set(GLint location, const glm::vec2& value) const;
	void set(GLint location, const glm::vec3& value) const;
	void set(GLint location, const glm::vec4& value) const;
	void set(GLint location, const glm::ivec2& value) const;
	void set(GLint location, const glm::ivec3& value) const;
	void set(GLint location, const glm::ivec4& value) const;
	void set(GLint location, const glm::mat3& value) const;
	void set(GLint location, const glm::mat4& value) const;

	/// Named form. Throws if the uniform does not exist.
	template<class T>
	void set(std::string_view name, const T& value) const {
		set(requireUniform(name), value);
	}

	/// Named form that silently does nothing if the uniform was optimized out.
	template<class T>
	void setIfPresent(std::string_view name, const T& value) const {
		if (const GLint location = uniformLocation(name); location >= 0) {
			set(location, value);
		}
	}

	/// Every file that contributed to this program, including `#include`d ones.
	/// This is the hot-reloader's watch list.
	[[nodiscard]] const std::vector<std::filesystem::path>& dependencies() const noexcept {
		return dependencies_;
	}

private:
	Program(ProgramHandle handle, std::vector<std::filesystem::path> dependencies);

	void assertBound() const;

	struct TransparentHash {
		using is_transparent = void;
		std::size_t operator()(std::string_view key) const noexcept {
			return std::hash<std::string_view>{}(key);
		}
	};

	ProgramHandle handle_;
	std::vector<std::filesystem::path> dependencies_;
	mutable std::unordered_map<std::string, GLint, TransparentHash, std::equal_to<>> uniformCache_;
};

} // namespace glc
