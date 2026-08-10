#pragma once

#include <glcore/handle.hpp>
#include <glcore/image.hpp>

#include <cstdint>
#include <filesystem>

// Texture and sampler creation, for the texturing chapters (14 onward).

namespace glc {

/// How the stored bytes should be interpreted.
///
/// Tutorial 16 (Gamma and Textures) hinges on this: image editors write sRGB,
/// but lighting maths needs linear values. Srgb asks GL to linearize on every
/// fetch, which costs nothing and is almost always what you want for color
/// textures. Use Linear for data textures -- normal maps, roughness, masks.
enum class ColorSpace : std::uint8_t { Linear, Srgb };

struct Texture2DOptions {
	ColorSpace colorSpace = ColorSpace::Linear;
	bool generateMipmaps = true;
};

/// Uploads pixel data as a 2D texture. Chooses the internal format from the
/// image's channel count and the requested color space.
[[nodiscard]] Texture makeTexture2D(const ImageData& image, const Texture2DOptions& options = {});

/// Loads a file and uploads it in one step.
[[nodiscard]] Texture loadTexture2D(const std::filesystem::path& path,
                                    const Texture2DOptions& options = {});

struct SamplerOptions {
	GLenum minFilter = GL_LINEAR_MIPMAP_LINEAR;
	GLenum magFilter = GL_LINEAR;
	GLenum wrapS = GL_REPEAT;
	GLenum wrapT = GL_REPEAT;

	/// 1.0 disables anisotropic filtering. Values above the driver's limit are
	/// clamped rather than rejected.
	float maxAnisotropy = 1.0F;
};

/// Sampler objects (GL 3.3) keep filtering separate from the texture, which is
/// exactly the split Tutorial 15 (Many Images) is about.
[[nodiscard]] Sampler makeSampler(const SamplerOptions& options = {});

/// Largest anisotropy this driver supports, or 1.0 if the extension is absent.
[[nodiscard]] float maxSupportedAnisotropy();

/// Binds `texture` and `sampler` to a texture unit together.
void bindTextureUnit(GLuint unit, const Texture& texture, const Sampler& sampler,
                     GLenum target = GL_TEXTURE_2D);

} // namespace glc
