#pragma once

#include <glcore/handle.hpp>
#include <glcore/vertex_array.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Meshes in gltut's XML format, needed from Tutorial 7 (World in Motion) --
// UnitPlane.xml, UnitCube.xml, UnitCylinder.xml, UnitSphere.xml and friends.
//
//     <mesh>
//       <attribute index="0" type="float" size="3"> ...numbers... </attribute>
//       <attribute index="2" type="float" size="4"> ...numbers... </attribute>
//       <vao name="lit"><source attrib="0"/><source attrib="2"/></vao>
//       <indices cmd="triangles" type="ushort"> ...numbers... </indices>
//       <arrays cmd="tri-fan" start="0" count="8"/>
//     </mesh>
//
// render() uses the main VAO, which carries every attribute. render(name) uses
// one of the named subsets, which is how the book swaps between a lit and an
// unlit program over the same geometry.

namespace glc {

namespace detail {

/// One draw call recorded by the file. Lives outside Mesh so the XML parsing
/// helpers in mesh.cpp can build these without being members.
struct MeshRenderCommand {
	GLenum primitive = GL_TRIANGLES;
	bool indexed = false;
	GLsizei count = 0;
	GLenum indexType = GL_UNSIGNED_SHORT; ///< indexed commands only
	std::size_t byteOffset = 0;           ///< into the index buffer
	GLint first = 0;                      ///< non-indexed commands only
};

} // namespace detail

class Mesh {
public:
	/// An empty mesh that draws nothing.
	///
	/// Unlike Program, where "not linked" is not a usable state, an empty mesh
	/// is perfectly meaningful, so this is public: a tutorial can declare
	/// `glc::Mesh mesh_;` as a member and assign to it in onInit() rather than
	/// wrapping it in std::optional.
	Mesh() = default;

	/// Throws on a malformed or missing file.
	[[nodiscard]] static Mesh fromXmlFile(const std::filesystem::path& path);

	/// Non-throwing form.
	[[nodiscard]] static std::expected<Mesh, std::string>
	tryFromXmlFile(const std::filesystem::path& path);

	/// Builds a mesh from interleaved vertex data held in memory.
	[[nodiscard]] static Mesh fromInterleaved(std::span<const std::byte> vertices,
	                                          std::span<const AttributeDesc> attributes,
	                                          GLsizei vertexCount,
	                                          std::span<const std::uint32_t> indices = {},
	                                          GLenum primitive = GL_TRIANGLES);

	/// Issues every render command using the main VAO.
	void render() const;

	/// Issues every render command using a named VAO. Throws if absent.
	void render(std::string_view vaoName) const;

	[[nodiscard]] bool hasVao(std::string_view name) const;
	[[nodiscard]] std::vector<std::string> vaoNames() const;

private:
	using RenderCommand = detail::MeshRenderCommand;

	struct NamedVao {
		std::string name;
		VertexArray vao;
	};

	void renderWith(const VertexArray& vao) const;

	Buffer attributeBuffer_;
	Buffer indexBuffer_;
	VertexArray mainVao_;
	std::vector<NamedVao> namedVaos_;
	std::vector<RenderCommand> commands_;

	friend class MeshBuilder;
};

} // namespace glc
