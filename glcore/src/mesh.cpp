#include <glcore/mesh.hpp>

#include <glcore/buffer.hpp>
#include <glcore/gl_check.hpp>
#include <glcore/scoped_bind.hpp>

#include <tinyxml2.h>

#include <algorithm>
#include <charconv>
#include <cstring>
#include <format>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace fs = std::filesystem;

namespace glc {
namespace {

// --- gltut type vocabulary -------------------------------------------------

struct TypeInfo {
	GLenum glType;
	std::size_t byteSize;
	bool normalized;
	/// Fed to glVertexAttribIPointer rather than glVertexAttribPointer, so the
	/// shader sees an int rather than a converted float.
	bool integral;
	/// Values are parsed as integers rather than floating point.
	bool parseAsInteger;
};

// gltut's attribute types fall into exactly three categories. Naming them beats
// repeating five positional booleans per entry.

/// Floating point in the file, float in the shader.
constexpr TypeInfo floatingType() {
	return {
	    .glType = GL_FLOAT,
	    .byteSize = 4,
	    .normalized = false,
	    .integral = false,
	    .parseAsInteger = false,
	};
}

/// Integer in the file, integer in the shader (glVertexAttribIPointer).
constexpr TypeInfo integerType(GLenum glType, std::size_t byteSize) {
	return {
	    .glType = glType,
	    .byteSize = byteSize,
	    .normalized = false,
	    .integral = true,
	    .parseAsInteger = true,
	};
}

/// Integer in the file, rescaled to a float in [0,1] or [-1,1] in the shader.
constexpr TypeInfo normalizedType(GLenum glType, std::size_t byteSize) {
	return {
	    .glType = glType,
	    .byteSize = byteSize,
	    .normalized = true,
	    .integral = false,
	    .parseAsInteger = true,
	};
}

std::optional<TypeInfo> lookupAttributeType(std::string_view name) {
	if (name == "float") {
		return floatingType();
	}
	if (name == "int") {
		return integerType(GL_INT, 4);
	}
	if (name == "uint") {
		return integerType(GL_UNSIGNED_INT, 4);
	}
	if (name == "short") {
		return integerType(GL_SHORT, 2);
	}
	if (name == "ushort") {
		return integerType(GL_UNSIGNED_SHORT, 2);
	}
	if (name == "byte") {
		return integerType(GL_BYTE, 1);
	}
	if (name == "ubyte") {
		return integerType(GL_UNSIGNED_BYTE, 1);
	}
	if (name == "norm-int") {
		return normalizedType(GL_INT, 4);
	}
	if (name == "norm-uint") {
		return normalizedType(GL_UNSIGNED_INT, 4);
	}
	if (name == "norm-short") {
		return normalizedType(GL_SHORT, 2);
	}
	if (name == "norm-ushort") {
		return normalizedType(GL_UNSIGNED_SHORT, 2);
	}
	if (name == "norm-byte") {
		return normalizedType(GL_BYTE, 1);
	}
	if (name == "norm-ubyte") {
		return normalizedType(GL_UNSIGNED_BYTE, 1);
	}
	return std::nullopt;
}

std::optional<TypeInfo> lookupIndexType(std::string_view name) {
	if (name == "uint") {
		return integerType(GL_UNSIGNED_INT, 4);
	}
	if (name == "ushort") {
		return integerType(GL_UNSIGNED_SHORT, 2);
	}
	if (name == "ubyte") {
		return integerType(GL_UNSIGNED_BYTE, 1);
	}
	return std::nullopt;
}

std::optional<GLenum> lookupPrimitive(std::string_view name) {
	if (name == "triangles") {
		return GL_TRIANGLES;
	}
	if (name == "tri-strip") {
		return GL_TRIANGLE_STRIP;
	}
	if (name == "tri-fan") {
		return GL_TRIANGLE_FAN;
	}
	if (name == "lines") {
		return GL_LINES;
	}
	if (name == "line-strip") {
		return GL_LINE_STRIP;
	}
	if (name == "line-loop") {
		return GL_LINE_LOOP;
	}
	if (name == "points") {
		return GL_POINTS;
	}
	return std::nullopt;
}

// --- number parsing --------------------------------------------------------

/// Splits on whitespace and parses each token, writing it into `out` in the
/// binary representation the attribute type calls for.
std::expected<std::size_t, std::string> parseNumbers(std::string_view text, const TypeInfo& type,
                                                     std::vector<std::byte>& out) {
	constexpr std::string_view kWhitespace = " \t\r\n";
	std::size_t count = 0;
	std::size_t cursor = 0;

	while (cursor < text.size()) {
		const std::size_t begin = text.find_first_not_of(kWhitespace, cursor);
		if (begin == std::string_view::npos) {
			break;
		}
		std::size_t end = text.find_first_of(kWhitespace, begin);
		if (end == std::string_view::npos) {
			end = text.size();
		}
		const std::string_view token = text.substr(begin, end - begin);
		cursor = end;

		std::array<std::byte, 8> scratch{};
		if (type.parseAsInteger) {
			long long value = 0;
			const auto [ptr, ec] =
			    std::from_chars(token.data(), token.data() + token.size(), value);
			if (ec != std::errc{} || ptr != token.data() + token.size()) {
				return std::unexpected(std::format("'{}' is not an integer", token));
			}
			switch (type.byteSize) {
				case 1: {
					const auto narrowed = static_cast<std::int8_t>(value);
					std::memcpy(scratch.data(), &narrowed, 1);
					break;
				}
				case 2: {
					const auto narrowed = static_cast<std::int16_t>(value);
					std::memcpy(scratch.data(), &narrowed, 2);
					break;
				}
				default: {
					const auto narrowed = static_cast<std::int32_t>(value);
					std::memcpy(scratch.data(), &narrowed, 4);
					break;
				}
			}
		} else {
			float value = 0.0F;
			const auto [ptr, ec] =
			    std::from_chars(token.data(), token.data() + token.size(), value);
			if (ec != std::errc{} || ptr != token.data() + token.size()) {
				return std::unexpected(std::format("'{}' is not a number", token));
			}
			std::memcpy(scratch.data(), &value, sizeof(float));
		}

		out.insert(out.end(), scratch.begin(), scratch.begin() + type.byteSize);
		++count;
	}

	return count;
}

// --- parsed intermediate ---------------------------------------------------

struct ParsedAttribute {
	GLuint index = 0;
	GLint size = 0;
	TypeInfo type{};
	std::size_t elementCount = 0; ///< number of vertices
	std::size_t byteOffset = 0;   ///< into the combined attribute buffer
	std::vector<std::byte> data;
};

const char* attributeOrNull(const tinyxml2::XMLElement& element, const char* name) {
	return element.Attribute(name);
}

std::expected<int, std::string> requiredInt(const tinyxml2::XMLElement& element, const char* name) {
	int value = 0;
	if (element.QueryIntAttribute(name, &value) != tinyxml2::XML_SUCCESS) {
		return std::unexpected(
		    std::format("<{}> is missing a valid '{}' attribute", element.Name(), name));
	}
	return value;
}

// --- parsing, one XML element at a time ------------------------------------

std::expected<ParsedAttribute, std::string> parseAttribute(const tinyxml2::XMLElement& node) {
	const auto index = requiredInt(node, "index");
	if (!index) {
		return std::unexpected(index.error());
	}
	const auto size = requiredInt(node, "size");
	if (!size) {
		return std::unexpected(size.error());
	}
	if (*size < 1 || *size > 4) {
		return std::unexpected(
		    std::format("<attribute index=\"{}\"> has size {}, which must be 1..4", *index, *size));
	}

	const char* typeName = attributeOrNull(node, "type");
	if (typeName == nullptr) {
		return std::unexpected(std::format("<attribute index=\"{}\"> is missing 'type'", *index));
	}
	const std::optional<TypeInfo> type = lookupAttributeType(typeName);
	if (!type) {
		return std::unexpected(
		    std::format("<attribute index=\"{}\"> has unsupported type '{}'", *index, typeName));
	}

	ParsedAttribute parsed;
	parsed.index = static_cast<GLuint>(*index);
	parsed.size = *size;
	parsed.type = *type;

	const char* text = node.GetText();
	const auto count = parseNumbers(text == nullptr ? "" : text, *type, parsed.data);
	if (!count) {
		return std::unexpected(std::format("<attribute index=\"{}\">: {}", *index, count.error()));
	}
	if (*count == 0 || *count % static_cast<std::size_t>(*size) != 0) {
		return std::unexpected(
		    std::format("<attribute index=\"{}\"> has {} values, not a multiple of size {}", *index,
			            *count, *size));
	}
	parsed.elementCount = *count / static_cast<std::size_t>(*size);
	return parsed;
}

/// Parses every <attribute>, checks they agree on the vertex count, and assigns
/// each one its offset into the single combined buffer.
std::expected<std::vector<ParsedAttribute>, std::string>
parseAttributes(const tinyxml2::XMLElement& root, const fs::path& path) {
	std::vector<ParsedAttribute> attributes;
	for (const tinyxml2::XMLElement* node = root.FirstChildElement("attribute"); node != nullptr;
	     node = node->NextSiblingElement("attribute")) {
		auto parsed = parseAttribute(*node);
		if (!parsed) {
			return std::unexpected(parsed.error());
		}
		attributes.push_back(std::move(*parsed));
	}

	if (attributes.empty()) {
		return std::unexpected(std::format("'{}' declares no <attribute>", path.string()));
	}

	const std::size_t vertexCount = attributes.front().elementCount;
	for (const ParsedAttribute& attribute : attributes) {
		if (attribute.elementCount != vertexCount) {
			return std::unexpected(
			    std::format("attribute {} has {} vertices but attribute {} has {}", attribute.index,
				            attribute.elementCount, attributes.front().index, vertexCount));
		}
	}
	return attributes;
}

std::expected<detail::MeshRenderCommand, std::string>
parseIndicesCommand(const tinyxml2::XMLElement& node, std::vector<std::byte>& indexBytes) {
	const char* primitiveName = attributeOrNull(node, "cmd");
	const char* typeName = attributeOrNull(node, "type");
	if (primitiveName == nullptr || typeName == nullptr) {
		return std::unexpected("<indices> needs both 'cmd' and 'type'");
	}
	const std::optional<GLenum> primitive = lookupPrimitive(primitiveName);
	if (!primitive) {
		return std::unexpected(std::format("<indices> has unknown cmd '{}'", primitiveName));
	}
	const std::optional<TypeInfo> type = lookupIndexType(typeName);
	if (!type) {
		return std::unexpected(std::format("<indices> has unsupported type '{}'", typeName));
	}

	const std::size_t offset = indexBytes.size();
	const char* text = node.GetText();
	const auto count = parseNumbers(text == nullptr ? "" : text, *type, indexBytes);
	if (!count) {
		return std::unexpected(std::format("<indices>: {}", count.error()));
	}

	return detail::MeshRenderCommand{
	    .primitive = *primitive,
	    .indexed = true,
	    .count = static_cast<GLsizei>(*count),
	    .indexType = type->glType,
	    .byteOffset = offset,
	    .first = 0,
	};
}

std::expected<detail::MeshRenderCommand, std::string>
parseArraysCommand(const tinyxml2::XMLElement& node) {
	const char* primitiveName = attributeOrNull(node, "cmd");
	if (primitiveName == nullptr) {
		return std::unexpected("<arrays> needs a 'cmd'");
	}
	const std::optional<GLenum> primitive = lookupPrimitive(primitiveName);
	if (!primitive) {
		return std::unexpected(std::format("<arrays> has unknown cmd '{}'", primitiveName));
	}
	const auto start = requiredInt(node, "start");
	if (!start) {
		return std::unexpected(start.error());
	}
	const auto count = requiredInt(node, "count");
	if (!count) {
		return std::unexpected(count.error());
	}

	return detail::MeshRenderCommand{
	    .primitive = *primitive,
	    .indexed = false,
	    .count = static_cast<GLsizei>(*count),
	    .indexType = GL_UNSIGNED_SHORT,
	    .byteOffset = 0,
	    .first = *start,
	};
}

/// Walks the children in document order so commands run in the order written.
std::expected<std::vector<detail::MeshRenderCommand>, std::string>
parseRenderCommands(const tinyxml2::XMLElement& root, const fs::path& path,
                    std::vector<std::byte>& indexBytes) {
	std::vector<detail::MeshRenderCommand> commands;

	for (const tinyxml2::XMLElement* node = root.FirstChildElement(); node != nullptr;
	     node = node->NextSiblingElement()) {
		const std::string_view name = node->Name();

		if (name == "indices") {
			auto command = parseIndicesCommand(*node, indexBytes);
			if (!command) {
				return std::unexpected(command.error());
			}
			commands.push_back(*command);
		} else if (name == "arrays") {
			auto command = parseArraysCommand(*node);
			if (!command) {
				return std::unexpected(command.error());
			}
			commands.push_back(*command);
		}
	}

	if (commands.empty()) {
		return std::unexpected(
		    std::format("'{}' declares no <indices> or <arrays> render command", path.string()));
	}
	return commands;
}

/// Concatenates the attribute arrays into one blob, recording each attribute's
/// offset as it goes.
std::vector<std::byte> packAttributes(std::vector<ParsedAttribute>& attributes) {
	std::vector<std::byte> bytes;
	for (ParsedAttribute& attribute : attributes) {
		attribute.byteOffset = bytes.size();
		bytes.insert(bytes.end(), attribute.data.begin(), attribute.data.end());
	}
	return bytes;
}

AttributeDesc describeAttribute(const ParsedAttribute& attribute) {
	return AttributeDesc{
	    .location = attribute.index,
	    .components = attribute.size,
	    .type = attribute.type.glType,
	    .normalized = attribute.type.normalized,
	    .stride = 0, // each attribute array is tightly packed
	    .offset = attribute.byteOffset,
	    .integer = attribute.type.integral,
	};
}

struct ParsedVao {
	std::string name;
	std::vector<AttributeDesc> attributes;
};

/// Resolves one <vao name="..."><source attrib="N"/>...</vao> into the subset of
/// attributes it names.
std::expected<ParsedVao, std::string> parseVao(const tinyxml2::XMLElement& node,
                                               const std::vector<ParsedAttribute>& attributes) {
	const char* vaoName = attributeOrNull(node, "name");
	if (vaoName == nullptr) {
		return std::unexpected("<vao> is missing a 'name'");
	}

	ParsedVao parsed;
	parsed.name = vaoName;

	for (const tinyxml2::XMLElement* source = node.FirstChildElement("source"); source != nullptr;
	     source = source->NextSiblingElement("source")) {
		const auto attrib = requiredInt(*source, "attrib");
		if (!attrib) {
			return std::unexpected(attrib.error());
		}
		const auto found = std::ranges::find_if(attributes, [&](const ParsedAttribute& candidate) {
			return std::cmp_equal(candidate.index, *attrib);
		});
		if (found == attributes.end()) {
			return std::unexpected(
			    std::format("<vao name=\"{}\"> references attribute {}, which is not declared",
				            vaoName, *attrib));
		}
		parsed.attributes.push_back(describeAttribute(*found));
	}

	return parsed;
}

} // namespace

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

std::expected<Mesh, std::string> Mesh::tryFromXmlFile(const fs::path& path) {
	tinyxml2::XMLDocument document;
	if (document.LoadFile(path.string().c_str()) != tinyxml2::XML_SUCCESS) {
		return std::unexpected(
		    std::format("cannot load mesh '{}': {}", path.string(), document.ErrorStr()));
	}

	const tinyxml2::XMLElement* root = document.FirstChildElement("mesh");
	if (root == nullptr) {
		return std::unexpected(std::format("'{}' has no <mesh> root element", path.string()));
	}

	auto attributes = parseAttributes(*root, path);
	if (!attributes) {
		return std::unexpected(attributes.error());
	}

	std::vector<std::byte> indexBytes;
	auto commands = parseRenderCommands(*root, path, indexBytes);
	if (!commands) {
		return std::unexpected(commands.error());
	}

	const std::vector<std::byte> attributeBytes = packAttributes(*attributes);

	// --- upload ------------------------------------------------------------
	Mesh mesh;
	mesh.commands_ = std::move(*commands);

	mesh.attributeBuffer_ = Buffer::create();
	bufferData(mesh.attributeBuffer_, GL_ARRAY_BUFFER, attributeBytes.data(), attributeBytes.size(),
	           GL_STATIC_DRAW);

	const bool hasIndices = !indexBytes.empty();
	if (hasIndices) {
		mesh.indexBuffer_ = Buffer::create();
		bufferData(mesh.indexBuffer_, GL_ELEMENT_ARRAY_BUFFER, indexBytes.data(), indexBytes.size(),
		           GL_STATIC_DRAW);
	}

	const Buffer* elements = hasIndices ? &mesh.indexBuffer_ : nullptr;

	std::vector<AttributeDesc> allDescs;
	allDescs.reserve(attributes->size());
	for (const ParsedAttribute& attribute : *attributes) {
		allDescs.push_back(describeAttribute(attribute));
	}
	mesh.mainVao_ = makeVertexArray(mesh.attributeBuffer_, allDescs, elements);

	// --- named VAOs (attribute subsets) ------------------------------------
	for (const tinyxml2::XMLElement* node = root->FirstChildElement("vao"); node != nullptr;
	     node = node->NextSiblingElement("vao")) {
		auto vao = parseVao(*node, *attributes);
		if (!vao) {
			return std::unexpected(vao.error());
		}
		mesh.namedVaos_.push_back(NamedVao{
		    .name = std::move(vao->name),
		    .vao = makeVertexArray(mesh.attributeBuffer_, vao->attributes, elements),
		});
	}

	return mesh;
}

Mesh Mesh::fromXmlFile(const fs::path& path) {
	auto mesh = tryFromXmlFile(path);
	if (!mesh) {
		throw std::runtime_error(mesh.error());
	}
	return std::move(*mesh);
}

Mesh Mesh::fromInterleaved(std::span<const std::byte> vertices,
                           std::span<const AttributeDesc> attributes, GLsizei vertexCount,
                           std::span<const std::uint32_t> indices, GLenum primitive) {
	Mesh mesh;

	mesh.attributeBuffer_ = Buffer::create();
	bufferData(mesh.attributeBuffer_, GL_ARRAY_BUFFER, vertices.data(), vertices.size_bytes(),
	           GL_STATIC_DRAW);

	const bool hasIndices = !indices.empty();
	if (hasIndices) {
		mesh.indexBuffer_ = Buffer::create();
		bufferData(mesh.indexBuffer_, GL_ELEMENT_ARRAY_BUFFER, indices.data(), indices.size_bytes(),
		           GL_STATIC_DRAW);
	}

	mesh.mainVao_ = makeVertexArray(mesh.attributeBuffer_, attributes,
	                                hasIndices ? &mesh.indexBuffer_ : nullptr);

	mesh.commands_.push_back(RenderCommand{
	    .primitive = primitive,
	    .indexed = hasIndices,
	    .count = hasIndices ? static_cast<GLsizei>(indices.size()) : vertexCount,
	    .indexType = GL_UNSIGNED_INT,
	    .byteOffset = 0,
	    .first = 0,
	});

	return mesh;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void Mesh::renderWith(const VertexArray& vao) const {
	const ScopedBind bind{vao};
	for (const RenderCommand& command : commands_) {
		if (command.indexed) {
			// glDrawElements takes its index-buffer offset as a pointer-typed
			// integer; the cast is imposed by the GL API, not a choice.
			const auto address = static_cast<std::uintptr_t>(command.byteOffset);
			const void* offset = reinterpret_cast<const void*>(address); // NOLINT
			GLC_CHECK(glDrawElements(command.primitive, command.count, command.indexType, offset));
		} else {
			GLC_CHECK(glDrawArrays(command.primitive, command.first, command.count));
		}
	}
}

void Mesh::render() const {
	renderWith(mainVao_);
}

void Mesh::render(std::string_view vaoName) const {
	const auto found = std::ranges::find_if(
	    namedVaos_, [&](const NamedVao& entry) { return entry.name == vaoName; });
	if (found == namedVaos_.end()) {
		throw std::runtime_error(std::format("mesh has no VAO named '{}'", vaoName));
	}
	renderWith(found->vao);
}

bool Mesh::hasVao(std::string_view name) const {
	return std::ranges::any_of(namedVaos_,
	                           [&](const NamedVao& entry) { return entry.name == name; });
}

std::vector<std::string> Mesh::vaoNames() const {
	std::vector<std::string> names;
	names.reserve(namedVaos_.size());
	for (const NamedVao& entry : namedVaos_) {
		names.push_back(entry.name);
	}
	return names;
}

} // namespace glc
