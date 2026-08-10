#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

// Image loading, backed by stb_image.
//
// The book loads its texture data through glimg's stb backend, so the same file
// formats work here: PNG, JPEG, TGA, BMP, PSD, GIF, HDR, PIC.
//
// Not covered: the handful of .dds assets in the later texturing chapters. DDS
// is a container for pre-compressed/mipmapped data that stb does not read; if a
// chapter needs one, converting it to PNG is usually simpler than adding a DDS
// reader.

namespace glc {

struct ImageData {
	int width = 0;
	int height = 0;
	int channels = 0;              ///< 1 = R, 2 = RG, 3 = RGB, 4 = RGBA
	std::vector<std::byte> pixels; ///< 8 bits per channel, tightly packed

	[[nodiscard]] bool empty() const noexcept {
		return pixels.empty();
	}
};

/// Loads an image. `desiredChannels` of 0 keeps the file's own channel count.
///
/// `flipVertically` defaults to true because image files store the top row
/// first while OpenGL's texture origin is bottom-left; without it every texture
/// appears upside down.
[[nodiscard]] std::expected<ImageData, std::string> tryLoadImage(const std::filesystem::path& path,
                                                                 int desiredChannels = 0,
                                                                 bool flipVertically = true);

/// Throwing form.
[[nodiscard]] ImageData loadImage(const std::filesystem::path& path, int desiredChannels = 0,
                                  bool flipVertically = true);

} // namespace glc
