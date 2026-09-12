#include "materials.hpp"

namespace {

/// Rounds `size` up to the next multiple of `alignment`.
///
/// gltut writes this as `size += alignment - (size % alignment)`, which is
/// wrong whenever size is already aligned: the remainder is 0, so it adds a
/// whole extra alignment and wastes a full slot per material. Harmless in the
/// book because 48 is never a multiple of 256, but it is the kind of thing that
/// starts costing memory the moment a struct grows.
[[nodiscard]] constexpr std::size_t alignUp(std::size_t size, std::size_t alignment) {
	return ((size + alignment - 1) / alignment) * alignment;
}

/// Bytes between consecutive materials. A free function so the member
/// initializer stays on one line -- readability-trailing-comma wants a trailing
/// comma in any braced initializer clang-format decides to wrap.
[[nodiscard]] std::size_t materialStride() {
	return alignUp(sizeof(MaterialBlock),
	               static_cast<std::size_t>(glc::UniformBuffer::offsetAlignment()));
}

/// Indices match MaterialId, and the draw order in onRender.
///
/// specularShininess values are Gaussian widths -- gaussianTerm() in
/// common/specular.glsl, not phongTerm().
constexpr std::array<MaterialBlock, kMaterialCount> kMaterials{
    {
        {
            // Ground. gltut gives the terrain a specular colour and then draws
            // it with a diffuse-only program, so this value never reaches a
            // shader there. Here it does; set it to zero to match the book.
            .diffuseColor = {1.0F, 1.0F, 1.0F, 1.0F},
            .specularColor = {0.5F, 0.5F, 0.5F, 1.0F},
            .specularShininess = 0.6F,
            .padding = {},
        },
        {
            .diffuseColor = {0.5F, 0.5F, 0.5F, 1.0F},
            .specularColor = {0.5F, 0.5F, 0.5F, 1.0F},
            .specularShininess = 0.05F, // tight highlight
            .padding = {},
        },
        {
            // Monolith: nearly black, nearly mirror. Its whole appearance is
            // the specular term, which makes it the object to watch when the
            // HDR stage starts clipping.
            .diffuseColor = {0.05F, 0.05F, 0.05F, 1.0F},
            .specularColor = {0.95F, 0.95F, 0.95F, 1.0F},
            .specularShininess = 0.4F,
            .padding = {},
        },
        {
            .diffuseColor = {0.5F, 0.5F, 0.5F, 1.0F},
            .specularColor = {0.3F, 0.3F, 0.3F, 1.0F},
            .specularShininess = 0.1F,
            .padding = {},
        },
        {
            // Cylinder: matte by data rather than by a second shader.
            .diffuseColor = {0.5F, 0.5F, 0.5F, 1.0F},
            .specularColor = {0.0F, 0.0F, 0.0F, 1.0F},
            .specularShininess = 0.6F,
            .padding = {},
        },
        {
            .diffuseColor = {0.63F, 0.60F, 0.02F, 1.0F},
            .specularColor = {0.22F, 0.20F, 0.0F, 1.0F},
            .specularShininess = 0.3F,
            .padding = {},
        },
    },
};

} // namespace

MaterialSet::MaterialSet()
    : stride_{materialStride()},
      buffer_{glc::UniformBuffer::create(stride_ * kMaterialCount, GL_STATIC_DRAW)} {
	// The buffer is allocated empty, so the contents go in here rather than in
	// the initializer list.
	for (std::size_t i = 0; i < kMaterials.size(); ++i) {
		buffer_.update(kMaterials[i], i * stride_);
	}
}

void MaterialSet::bind(GLuint bindingPoint, MaterialId id) const {
	// Only sizeof(MaterialBlock) is exposed, not the whole padded slot -- the
	// shader must not see the dead bytes that follow.
	buffer_.bindRange(bindingPoint, static_cast<std::size_t>(id) * stride_, sizeof(MaterialBlock));
}
