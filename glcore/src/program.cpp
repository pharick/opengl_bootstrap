#include <glcore/program.hpp>

#include <glcore/gl_check.hpp>
#include <glcore/glsl_source.hpp>
#include <glcore/log.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>
#include <utility>

namespace fs = std::filesystem;

namespace glc {
namespace {

std::string_view stageName(GLenum stage) {
	switch (stage) {
		case GL_VERTEX_SHADER:
			return "vertex";
		case GL_FRAGMENT_SHADER:
			return "fragment";
		case GL_GEOMETRY_SHADER:
			return "geometry";
		default:
			return "unknown";
	}
}

std::string shaderInfoLog(GLuint shader) {
	GLint length = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
	if (length <= 0) {
		return {};
	}
	std::string log(static_cast<std::size_t>(length), '\0');
	glGetShaderInfoLog(shader, length, nullptr, log.data());
	while (!log.empty() && (log.back() == '\0' || log.back() == '\n')) {
		log.pop_back();
	}
	return log;
}

std::string programInfoLog(GLuint program) {
	GLint length = 0;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
	if (length <= 0) {
		return {};
	}
	std::string log(static_cast<std::size_t>(length), '\0');
	glGetProgramInfoLog(program, length, nullptr, log.data());
	while (!log.empty() && (log.back() == '\0' || log.back() == '\n')) {
		log.pop_back();
	}
	return log;
}

std::expected<ShaderHandle, std::string> compileStage(const ShaderSource& source,
                                                      std::vector<fs::path>& dependencies) {
	auto loaded = loadGlslSource(source.path);
	if (!loaded) {
		return std::unexpected(std::format("{} shader '{}': {}", stageName(source.stage),
		                                   source.path.filename().string(), loaded.error()));
	}

	for (const fs::path& file : loaded->files) {
		if (std::ranges::find(dependencies, file) == dependencies.end()) {
			dependencies.push_back(file);
		}
	}

	ShaderHandle shader = ShaderHandle::adopt(glCreateShader(source.stage));
	if (!shader) {
		return std::unexpected(
		    std::format("glCreateShader failed for the {} stage", stageName(source.stage)));
	}

	const char* text = loaded->text.c_str();
	const auto length = static_cast<GLint>(loaded->text.size());
	glShaderSource(shader.id(), 1, &text, &length);
	glCompileShader(shader.id());

	GLint status = GL_FALSE;
	glGetShaderiv(shader.id(), GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		return std::unexpected(std::format("{} shader '{}' failed to compile:\n{}\n  sources:{}",
		                                   stageName(source.stage), source.path.filename().string(),
		                                   shaderInfoLog(shader.id()),
		                                   formatSourceLegend(loaded->files)));
	}
	return shader;
}

} // namespace

Program::Program(ProgramHandle handle, std::vector<fs::path> dependencies)
    : handle_{std::move(handle)}, dependencies_{std::move(dependencies)} {}

std::expected<Program, std::string> Program::tryFromFiles(std::span<const ShaderSource> sources) {
	if (sources.empty()) {
		return std::unexpected("a program needs at least one shader stage");
	}

	std::vector<fs::path> dependencies;
	std::vector<ShaderHandle> shaders;
	shaders.reserve(sources.size());

	for (const ShaderSource& source : sources) {
		auto shader = compileStage(source, dependencies);
		if (!shader) {
			return std::unexpected(shader.error());
		}
		shaders.push_back(std::move(*shader));
	}

	ProgramHandle program = ProgramHandle::create();
	if (!program) {
		return std::unexpected("glCreateProgram failed");
	}

	for (const ShaderHandle& shader : shaders) {
		glAttachShader(program.id(), shader.id());
	}
	glLinkProgram(program.id());

	// Detach so the shader objects are freed as soon as `shaders` goes away.
	for (const ShaderHandle& shader : shaders) {
		glDetachShader(program.id(), shader.id());
	}

	GLint status = GL_FALSE;
	glGetProgramiv(program.id(), GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		return std::unexpected(
		    std::format("program failed to link:\n{}", programInfoLog(program.id())));
	}

	return Program{std::move(program), std::move(dependencies)};
}

Program Program::fromFiles(std::span<const ShaderSource> sources) {
	auto program = tryFromFiles(sources);
	if (!program) {
		throw std::runtime_error(program.error());
	}
	return std::move(*program);
}

Program Program::fromFiles(const fs::path& vertex, const fs::path& fragment) {
	const std::array<ShaderSource, 2> sources{
	    ShaderSource{.stage = GL_VERTEX_SHADER, .path = vertex},
	    ShaderSource{.stage = GL_FRAGMENT_SHADER, .path = fragment},
	};
	return fromFiles(sources);
}

void Program::use() const {
	glUseProgram(handle_.id());
}

void Program::unuse() {
	glUseProgram(0);
}

GLint Program::uniformLocation(std::string_view name) const {
	if (const auto it = uniformCache_.find(name); it != uniformCache_.end()) {
		return it->second;
	}
	const std::string key{name};
	const GLint location = glGetUniformLocation(handle_.id(), key.c_str());
	uniformCache_.emplace(key, location);
	return location;
}

GLint Program::requireUniform(std::string_view name) const {
	const GLint location = uniformLocation(name);
	if (location < 0) {
		throw std::runtime_error(
		    std::format("uniform '{}' not found in program {} (misspelled, or unused and "
		                "therefore optimized out by the GLSL compiler)",
		                name, handle_.id()));
	}
	return location;
}

void Program::bindUniformBlock(std::string_view blockName, GLuint bindingPoint) const {
	const std::string key{blockName};
	const GLuint index = glGetUniformBlockIndex(handle_.id(), key.c_str());
	if (index == GL_INVALID_INDEX) {
		throw std::runtime_error(
		    std::format("uniform block '{}' not found in program {}", blockName, handle_.id()));
	}
	GLC_CHECK(glUniformBlockBinding(handle_.id(), index, bindingPoint));
}

void Program::assertBound() const {
#ifdef GLC_DEBUG
	GLint current = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &current);
	if (static_cast<GLuint>(current) != handle_.id()) {
		throw GlError(
		    std::format("uniform set on program {} while program {} is bound -- call use() first",
		                handle_.id(), current));
	}
#endif
}

void Program::set(GLint location, bool value) const {
	assertBound();
	glUniform1i(location, value ? 1 : 0);
}

void Program::set(GLint location, int value) const {
	assertBound();
	glUniform1i(location, value);
}

void Program::set(GLint location, unsigned int value) const {
	assertBound();
	glUniform1ui(location, value);
}

void Program::set(GLint location, float value) const {
	assertBound();
	glUniform1f(location, value);
}

void Program::set(GLint location, const glm::vec2& value) const {
	assertBound();
	glUniform2fv(location, 1, glm::value_ptr(value));
}

void Program::set(GLint location, const glm::vec3& value) const {
	assertBound();
	glUniform3fv(location, 1, glm::value_ptr(value));
}

void Program::set(GLint location, const glm::vec4& value) const {
	assertBound();
	glUniform4fv(location, 1, glm::value_ptr(value));
}

void Program::set(GLint location, const glm::ivec2& value) const {
	assertBound();
	glUniform2iv(location, 1, glm::value_ptr(value));
}

void Program::set(GLint location, const glm::ivec3& value) const {
	assertBound();
	glUniform3iv(location, 1, glm::value_ptr(value));
}

void Program::set(GLint location, const glm::ivec4& value) const {
	assertBound();
	glUniform4iv(location, 1, glm::value_ptr(value));
}

void Program::set(GLint location, const glm::mat3& value) const {
	assertBound();
	glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(value));
}

void Program::set(GLint location, const glm::mat4& value) const {
	assertBound();
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
}

} // namespace glc
