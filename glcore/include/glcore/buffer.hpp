#pragma once

#include <glcore/handle.hpp>

#include <cstddef>
#include <ranges>

namespace glc {

/// Allocates and fills a buffer's data store. Binds `target`, uploads, unbinds.
void bufferData(const Buffer& buffer, GLenum target, const void* data, std::size_t bytes,
                GLenum usage);

/// Range overload: accepts std::array, std::vector, std::span, C arrays.
template<std::ranges::contiguous_range R>
void bufferData(const Buffer& buffer, GLenum target, const R& data, GLenum usage = GL_STATIC_DRAW) {
	using Value = std::ranges::range_value_t<R>;
	bufferData(buffer, target, std::ranges::data(data),
	           static_cast<std::size_t>(std::ranges::size(data)) * sizeof(Value), usage);
}

/// Convenience: create a buffer and fill it in one step.
template<std::ranges::contiguous_range R>
[[nodiscard]] Buffer makeBuffer(GLenum target, const R& data, GLenum usage = GL_STATIC_DRAW) {
	Buffer buffer = Buffer::create();
	bufferData(buffer, target, data, usage);
	return buffer;
}

/// Replaces a sub-range of an existing data store.
void bufferSubData(const Buffer& buffer, GLenum target, std::size_t offsetBytes, const void* data,
                   std::size_t bytes);

} // namespace glc
