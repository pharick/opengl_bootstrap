#include <glcore/texture.hpp>

#include <glcore/gl_check.hpp>
#include <glcore/log.hpp>

#include <algorithm>
#include <format>
#include <stdexcept>

namespace fs = std::filesystem;

namespace glc {
namespace {

struct Formats {
	GLenum internalFormat;
	GLenum pixelFormat;
};

Formats chooseFormats(int channels, ColorSpace colorSpace) {
	const bool srgb = colorSpace == ColorSpace::Srgb;
	switch (channels) {
		case 1:
			if (srgb) {
				log::warn("sRGB is not defined for a 1-channel texture; using linear R8");
			}
			return {.internalFormat = GL_R8, .pixelFormat = GL_RED};
		case 2:
			if (srgb) {
				log::warn("sRGB is not defined for a 2-channel texture; using linear RG8");
			}
			return {.internalFormat = GL_RG8, .pixelFormat = GL_RG};
		case 3:
			return {.internalFormat = static_cast<GLenum>(srgb ? GL_SRGB8 : GL_RGB8),
			        .pixelFormat = GL_RGB};
		case 4:
			return {.internalFormat = static_cast<GLenum>(srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8),
			        .pixelFormat = GL_RGBA};
		default:
			throw std::runtime_error(
			    std::format("unsupported channel count {} for a texture", channels));
	}
}

} // namespace

Texture makeTexture2D(const ImageData& image, const Texture2DOptions& options) {
	if (image.empty() || image.width <= 0 || image.height <= 0) {
		throw std::runtime_error("cannot create a texture from an empty image");
	}

	const Formats formats = chooseFormats(image.channels, options.colorSpace);

	Texture texture = Texture::create();
	glBindTexture(GL_TEXTURE_2D, texture.id());

	// Rows of a 3-channel image are not 4-byte aligned; the default unpack
	// alignment of 4 would shear the image.
	GLint previousAlignment = 4;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	GLC_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(formats.internalFormat),
	                       image.width, image.height, 0, formats.pixelFormat, GL_UNSIGNED_BYTE,
	                       image.pixels.data()));

	glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);

	if (options.generateMipmaps) {
		GLC_CHECK(glGenerateMipmap(GL_TEXTURE_2D));
	} else {
		// Without mipmaps the default GL_LINEAR_MIPMAP_LINEAR min filter would
		// make the texture incomplete and sample as black.
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	return texture;
}

Texture loadTexture2D(const fs::path& path, const Texture2DOptions& options) {
	return makeTexture2D(loadImage(path), options);
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
