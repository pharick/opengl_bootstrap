// A reference for the texturing chapters (14-17): texture loading, sampler
// objects, mipmaps, anisotropy and the linear-vs-sRGB distinction.
//
// The two quads show the same pixels as GL_RGB8 and as GL_SRGB8. Tutorial 16 is
// about why they differ. Note that these are two *files*: with KTX the internal
// format is authored into the asset, so the choice is made by
// tools/png_to_ktx.py --srgb rather than by a flag at upload time.

#include <glcore/app.hpp>
#include <glcore/buffer.hpp>
#include <glcore/paths.hpp>
#include <glcore/scoped_bind.hpp>
#include <glcore/texture.hpp>
#include <glcore/vertex_array.hpp>

#include <glm/ext/matrix_transform.hpp>

#include <imgui.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

constexpr GLuint kPositionLocation = 0;
constexpr GLuint kTexCoordLocation = 1;
constexpr GLuint kTextureUnit = 0;

struct Vertex {
	glm::vec2 position;
	glm::vec2 texCoord;
};

constexpr std::array<Vertex, 4> kQuad{
    {
        {.position = {-0.5F, -0.5F}, .texCoord = {0.0F, 0.0F}},
        {.position = {0.5F, -0.5F}, .texCoord = {1.0F, 0.0F}},
        {.position = {0.5F, 0.5F}, .texCoord = {1.0F, 1.0F}},
        {.position = {-0.5F, 0.5F}, .texCoord = {0.0F, 1.0F}},
    },
};

constexpr std::array<std::uint16_t, 6> kIndices{0, 1, 2, 0, 2, 3};

class TexturedQuad final : public glc::App {
public:
	TexturedQuad()
	    : glc::App({.window = {.title = "gltut 03 -- Textured Quad"}, .depthTest = false}) {}

protected:
	void onInit() override {
		program_ = &shaders().add(glc::paths::tutorialShader("quad.vert"),
		                          glc::paths::tutorialShader("quad.frag"));

		linear_ = glc::loadTexture2D(glc::paths::asset("textures/checker.ktx"));
		srgb_ = glc::loadTexture2D(glc::paths::asset("textures/checker_srgb.ktx"));
		glc::log::info("loaded checker.ktx (GL_RGB8) and checker_srgb.ktx (GL_SRGB8)");

		maxAnisotropy_ = glc::maxSupportedAnisotropy();
		glc::log::info("max anisotropy: {}", maxAnisotropy_);
		rebuildSampler();

		vbo_ = glc::makeBuffer(GL_ARRAY_BUFFER, kQuad);
		ebo_ = glc::makeBuffer(GL_ELEMENT_ARRAY_BUFFER, kIndices);
		vao_ = glc::makeVertexArray(vbo_,
		                            std::array{
		                                glc::AttributeDesc{
		                                    .location = kPositionLocation,
		                                    .components = 2,
		                                    .stride = sizeof(Vertex),
		                                    .offset = offsetof(Vertex, position),
		                                },
		                                glc::AttributeDesc{
		                                    .location = kTexCoordLocation,
		                                    .components = 2,
		                                    .stride = sizeof(Vertex),
		                                    .offset = offsetof(Vertex, texCoord),
		                                },
		                            },
		                            &ebo_);
	}

	void onRender() override {
		const glc::Program& program = program_->get();
		program.use();
		program.set("diffuse", static_cast<int>(kTextureUnit));
		program.set("tiling", tiling_);
		program.set("manualGamma", manualGamma_);

		const glc::ScopedBind bindVao{vao_};

		drawQuad(program, linear_, -0.55F);
		drawQuad(program, srgb_, 0.55F);
	}

	void onGui() override {
		if (ImGui::Begin("Tutorial 03")) {
			ImGui::Text("left: GL_RGB8 (linear)   right: GL_SRGB8");
			ImGui::SliderFloat("Tiling", &tiling_, 1.0F, 12.0F);
			if (ImGui::SliderFloat("Anisotropy", &anisotropy_, 1.0F, maxAnisotropy_)) {
				rebuildSampler();
			}
			if (ImGui::Checkbox("Mipmap filtering", &mipmapped_)) {
				rebuildSampler();
			}
			// Uses linearToSrgb() from assets/shaders/common/gamma.glsl,
			// pulled in by the shader's #include.
			ImGui::Checkbox("Manual gamma in shader", &manualGamma_);
		}
		ImGui::End();
	}

private:
	void rebuildSampler() {
		sampler_ = glc::makeSampler({
		    .minFilter = static_cast<GLenum>(mipmapped_ ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST),
		    .magFilter = GL_LINEAR,
		    .maxAnisotropy = anisotropy_,
		});
	}

	void drawQuad(const glc::Program& program, const glc::Texture& texture, float xOffset) const {
		glc::bindTextureUnit(kTextureUnit, texture, sampler_);
		const glm::mat4 model =
		    glm::translate(glm::mat4{1.0F}, glm::vec3{xOffset, 0.0F, 0.0F}) *
		    glm::scale(glm::mat4{1.0F}, glm::vec3{1.0F / aspect() * 1.6F, 1.6F, 1.0F});
		program.set("modelToClip", model);
		GLC_CHECK(glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(kIndices.size()),
		                         GL_UNSIGNED_SHORT, nullptr));
	}

	glc::ReloadableProgram* program_ = nullptr;
	glc::Texture linear_;
	glc::Texture srgb_;
	glc::Sampler sampler_;
	glc::Buffer vbo_;
	glc::Buffer ebo_;
	glc::VertexArray vao_;

	float tiling_ = 3.0F;
	float anisotropy_ = 1.0F;
	float maxAnisotropy_ = 1.0F;
	bool mipmapped_ = true;
	bool manualGamma_ = false;
};

} // namespace

int main() {
	return glc::runApp<TexturedQuad>();
}
