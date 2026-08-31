#include <glcore/texture.hpp>

#include <glcore/gl_check.hpp>
#include <glcore/log.hpp>

#include <gli/gl.hpp>
#include <gli/gli.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>

namespace fs = std::filesystem;

namespace glc {
namespace {

/// gli's swizzle enumerators are the GL enum values, so this is a type change
/// rather than a mapping. Applied so formats gli emulates (luminance, alpha)
/// sample correctly; it is the identity for ordinary RGB/RGBA.
void applySwizzle(const gli::gl::format& format) {
	const std::array<GLint, 4> swizzle{
	    static_cast<GLint>(format.Swizzles[0]),
	    static_cast<GLint>(format.Swizzles[1]),
	    static_cast<GLint>(format.Swizzles[2]),
	    static_cast<GLint>(format.Swizzles[3]),
	};
	glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle.data());
}

/// Without a mip chain, the default GL_LINEAR_MIPMAP_LINEAR minification filter
/// leaves the texture incomplete and it samples as black.
void finishMipLevels(bool hasChain, bool canGenerate) {
	if (hasChain) {
		return;
	}
	if (canGenerate) {
		GLC_CHECK(glGenerateMipmap(GL_TEXTURE_2D));
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	}
}

} // namespace

Texture loadTexture2D(const fs::path& path, const Texture2DOptions& options) {
	const gli::texture loaded = gli::load(path.string());
	if (loaded.empty()) {
		throw std::runtime_error(
		    std::format("cannot load texture '{}' -- gli reads .ktx and .dds only; convert "
		                "other formats with tools/png_to_ktx.py",
		                path.string()));
	}
	if (loaded.target() != gli::TARGET_2D) {
		throw std::runtime_error(
		    std::format("texture '{}' is not a plain 2D texture (cubemaps and arrays are not "
		                "wired up yet)",
		                path.string()));
	}

	const gli::texture2d texture{loaded};
	const gli::gl converter{gli::gl::PROFILE_GL33};
	const gli::gl::format format = converter.translate(texture.format(), texture.swizzles());
	const bool compressed = gli::is_compressed(texture.format());

	Texture handle = Texture::create();
	glBindTexture(GL_TEXTURE_2D, handle.id());

	// KTX stores uncompressed rows at GL_UNPACK_ALIGNMENT = 4, which is also
	// GL's default -- set it anyway so a caller who changed it cannot corrupt
	// the upload.
	GLint previousAlignment = 4;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(texture.levels() - 1));
	applySwizzle(format);

	for (std::size_t level = 0; level < texture.levels(); ++level) {
		const gli::texture2d::extent_type extent = texture.extent(level);
		const auto glLevel = static_cast<GLint>(level);

		if (compressed) {
			GLC_CHECK(glCompressedTexImage2D(
			    GL_TEXTURE_2D, glLevel, static_cast<GLenum>(format.Internal), extent.x, extent.y, 0,
			    static_cast<GLsizei>(texture.size(level)), texture.data(0, 0, level)));
		} else {
			GLC_CHECK(glTexImage2D(GL_TEXTURE_2D, glLevel, static_cast<GLint>(format.Internal),
			                       extent.x, extent.y, 0, static_cast<GLenum>(format.External),
			                       static_cast<GLenum>(format.Type), texture.data(0, 0, level)));
		}
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);

	// A compressed format's mips must be authored offline; the driver cannot
	// synthesize them.
	finishMipLevels(texture.levels() > 1, options.generateMipmaps && !compressed);

	glBindTexture(GL_TEXTURE_2D, 0);

	log::trace("loaded {}: {}x{}, {} level(s), internal format 0x{:04X}{}",
	           path.filename().string(), texture.extent().x, texture.extent().y, texture.levels(),
	           static_cast<unsigned>(format.Internal), compressed ? ", compressed" : "");

	return handle;
}

Texture makeTexture2D(GLsizei width, GLsizei height, GLenum internalFormat, GLenum format,
                      GLenum type, const void* pixels, const Texture2DOptions& options) {
	if (width <= 0 || height <= 0) {
		throw std::runtime_error(std::format("cannot create a {}x{} texture", width, height));
	}

	Texture handle = Texture::create();
	glBindTexture(GL_TEXTURE_2D, handle.id());

	// Rows of a tightly packed 3-channel image are not 4-byte aligned; the
	// default alignment of 4 would shear the image.
	GLint previousAlignment = 4;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	GLC_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat), width, height, 0,
	                       format, type, pixels));

	glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);

	finishMipLevels(false, options.generateMipmaps);

	glBindTexture(GL_TEXTURE_2D, 0);
	return handle;
}

float maxSupportedAnisotropy() {
	if (GLEW_EXT_texture_filter_anisotropic == GL_FALSE) {
		return 1.0F;
	}
	GLfloat limit = 1.0F;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &limit);
	return limit;
}

Sampler makeSampler(const SamplerOptions& options) {
	Sampler sampler = Sampler::create();
	const GLuint id = sampler.id();

	glSamplerParameteri(id, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(options.minFilter));
	glSamplerParameteri(id, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(options.magFilter));
	glSamplerParameteri(id, GL_TEXTURE_WRAP_S, static_cast<GLint>(options.wrapS));
	glSamplerParameteri(id, GL_TEXTURE_WRAP_T, static_cast<GLint>(options.wrapT));

	if (options.maxAnisotropy > 1.0F) {
		const float limit = maxSupportedAnisotropy();
		if (limit <= 1.0F) {
			log::warn("anisotropic filtering requested but EXT_texture_filter_anisotropic "
			          "is unavailable");
		} else {
			glSamplerParameterf(id, GL_TEXTURE_MAX_ANISOTROPY_EXT,
			                    std::min(options.maxAnisotropy, limit));
		}
	}

	GLC_CHECK(glSamplerParameteri(id, GL_TEXTURE_WRAP_R, static_cast<GLint>(options.wrapS)));
	return sampler;
}

void bindTextureUnit(GLuint unit, const Texture& texture, const Sampler& sampler, GLenum target) {
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(target, texture.id());
	GLC_CHECK(glBindSampler(unit, sampler.id()));
}

} // namespace glc
