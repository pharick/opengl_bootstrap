#pragma once

#include <glcore/gl.hpp>
#include <glcore/uniform_buffer.hpp>

#include <glm/vec4.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

/// std140 layout, same manual-padding rules as LightBlock.
struct MaterialBlock {
	glm::vec4 diffuseColor;
	glm::vec4 specularColor;
	float specularShininess; ///< Gaussian width in radians, not a Phong exponent
	std::array<float, 3> padding;
};
static_assert(sizeof(MaterialBlock) == 48);

/// Which slice of the material buffer an object draws with. Values are the
/// index into the buffer, so the order here is the order in kMaterials.
enum class MaterialId : std::uint8_t {
	Ground = 0,
	Tetrahedron,
	Monolith,
	Cube,
	Cylinder,
	Sphere,
};

constexpr std::size_t kMaterialCount = 6;

/// Every material in one buffer, each starting on an alignment boundary so a
/// single slice can be bound per draw with glBindBufferRange.
///
/// The padding is not optional. glBindBufferRange requires its offset to be a
/// multiple of GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, so six 48-byte blocks cannot
/// simply sit 48 bytes apart -- the second one would be rejected. On Apple
/// silicon the alignment is 256, so each 48-byte material occupies a 256-byte
/// slot and roughly four fifths of this buffer is dead space. That cost is why
/// later chapters stop packing materials this way.
class MaterialSet {
public:
	MaterialSet();

	/// Binds one material to `bindingPoint` for the next draw.
	void bind(GLuint bindingPoint, MaterialId id) const;

	/// Distance between consecutive materials, in bytes. Worth putting on
	/// screen next to sizeof(MaterialBlock).
	[[nodiscard]] std::size_t stride() const noexcept {
		return stride_;
	}

private:
	// Declaration order is load-bearing: buffer_'s initializer reads stride_,
	// and members initialize in declaration order regardless of how the
	// initializer list is written. Swapping these two compiles silently and
	// allocates a garbage-sized buffer.
	std::size_t stride_;
	glc::UniformBuffer buffer_;
};
