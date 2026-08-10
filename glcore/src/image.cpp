#include <glcore/image.hpp>

#include <stb_image.h>

#include <format>
#include <stdexcept>

namespace fs = std::filesystem;

namespace glc {

std::expected<ImageData, std::string> tryLoadImage(const fs::path& path, int desiredChannels,
                                                   bool flipVertically) {
	// stb's flip flag is global state, so it is set on every call rather than
	// once at startup.
	stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);

	int width = 0;
	int height = 0;
	int channelsInFile = 0;
	stbi_uc* raw =
	    stbi_load(path.string().c_str(), &width, &height, &channelsInFile, desiredChannels);

	if (raw == nullptr) {
		const char* reason = stbi_failure_reason();
		return std::unexpected(std::format("cannot load image '{}': {}", path.string(),
		                                   reason == nullptr ? "unknown error" : reason));
	}

	const int channels = desiredChannels > 0 ? desiredChannels : channelsInFile;
	const auto byteCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
	                       static_cast<std::size_t>(channels);

	ImageData image;
	image.width = width;
	image.height = height;
	image.channels = channels;
	image.pixels.assign(reinterpret_cast<const std::byte*>(raw),
	                    reinterpret_cast<const std::byte*>(raw) + byteCount);

	stbi_image_free(raw);
	return image;
}

ImageData loadImage(const fs::path& path, int desiredChannels, bool flipVertically) {
	auto image = tryLoadImage(path, desiredChannels, flipVertically);
	if (!image) {
		throw std::runtime_error(image.error());
	}
	return std::move(*image);
}

} // namespace glc
