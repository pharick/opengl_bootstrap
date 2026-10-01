// Tutorial 16 (Gamma and Textures): Gamma Ramp.
//
// Two copies of one picture: five bands of grey, black at the left and white
// at the right, drawn as screen-space quads one above the other. The question
// the chapter opens with is what those greys *mean*. The bytes are 0, 135,
// 186, 223 and 255, which look like nothing in particular -- until they are
// read as sRGB, where they are the encodings of 0, 0.25, 0.5, 0.75 and 1.0.
// The image is a linear ramp, stored the way images are stored: gamma-encoded.
//
// The top quad's shader writes the texel straight out; the bottom one raises
// it to 1/2.2 first, which is the gamma correction of Tutorial 12. Neither
// shader changes. What the keys change is the texture each one reads -- the
// same bytes, uploaded as GL_RGB8 or as GL_SRGB8:
//
//   - GL_RGB8 hands the bytes back as they are, divided by 255. The top quad
//     shows the image as it was authored, and the bottom one gamma-corrects
//     values that were already gamma-encoded, so it comes out washed out.
//
//   - GL_SRGB8 decodes on fetch: 135 comes back as 0.25. Now the top quad
//     writes linear values to a display that expects encoded ones, and the
//     ramp crushes towards black. The bottom one encodes them again, and is
//     back to the picture the artist drew -- but with the shader working in
//     linear space in between, which is where lighting has to happen.
//
// So top-linear and bottom-sRGB match, and that is the chapter's point: an
// sRGB texture plus gamma correction on output is the identity, with linear
// values available in the middle.
//
// 1 toggles the top quad's texture, 2 the bottom one's.

#include <glcore/app.hpp>
#include <glcore/buffer.hpp>
#include <glcore/paths.hpp>
#include <glcore/scoped_bind.hpp>
#include <glcore/texture.hpp>
#include <glcore/uniform_buffer.hpp>
#include <glcore/vertex_array.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

#include <imgui.h>

#include <array>
#include <cstddef>

namespace {

constexpr GLuint kProjectionBlockBinding = 0;
constexpr GLuint kGammaRampTextureUnit = 0;

constexpr GLuint kPositionLocation = 0;
constexpr GLuint kTexCoordLocation = 5; ///< gltut's slot for texture coordinates

struct ProjectionBlock {
	glm::mat4 cameraToClipMatrix;
};
static_assert(sizeof(ProjectionBlock) == 64);

/// Two quads as triangle strips, in window pixels from the top-left corner,
/// exactly as the book lays them out in its 500x192 window.
///
/// Both attributes are unsigned shorts. The position is converted to float as
/// is -- 90 stays 90.0, and the projection turns pixels into clip space. The
/// texture coordinate is *normalized*, so 65535 arrives as 1.0: a cheap way
/// to pack a coordinate that only ever needs to say "this edge" or "that one".
struct RampVertex {
	GLushort x;
	GLushort y;
	GLushort s;
	GLushort t;
};
static_assert(sizeof(RampVertex) == 8);

constexpr GLushort kTexMax = 65535;

constexpr std::array<RampVertex, 8> kVertices{
    {
        // top quad: no gamma correction in the shader
        {.x = 90, .y = 80, .s = 0, .t = 0},
        {.x = 90, .y = 16, .s = 0, .t = kTexMax},
        {.x = 410, .y = 80, .s = kTexMax, .t = 0},
        {.x = 410, .y = 16, .s = kTexMax, .t = kTexMax},
        // bottom quad: gamma-corrected in the shader
        {.x = 90, .y = 176, .s = 0, .t = 0},
        {.x = 90, .y = 112, .s = 0, .t = kTexMax},
        {.x = 410, .y = 176, .s = kTexMax, .t = 0},
        {.x = 410, .y = 112, .s = kTexMax, .t = kTexMax},
    },
};
constexpr GLint kQuadVertexCount = 4;

/// The book's gamma_ramp.png is 320x64 pixels of five 64-pixel bands, every
/// row the same. Under the nearest-texel filter five texels draw exactly the
/// same picture, so they are built here rather than shipped as a file.
///
/// The values are sRGB encodings of 0, 1/4, 1/2, 3/4 and 1 -- pow(x, 1/2.2)
/// times 255, truncated. Ask a linear texture for the second band and it
/// says 0.53; ask an sRGB one and it says 0.25.
constexpr std::size_t kBandCount = 5;
constexpr std::array<GLubyte, kBandCount> kBandValues{0, 135, 186, 223, 255};

[[nodiscard]] std::array<GLubyte, kBandCount * 3> rampPixels() {
	std::array<GLubyte, kBandCount * 3> pixels{};
	for (std::size_t band = 0; band < kBandCount; ++band) {
		pixels[(band * 3) + 0] = kBandValues[band];
		pixels[(band * 3) + 1] = kBandValues[band];
		pixels[(band * 3) + 2] = kBandValues[band];
	}
	return pixels;
}

/// One of the two quads: the shader it is drawn with never changes, the
/// texture it reads does.
struct Row {
	const char* label{};
	const char* shader{};
	int key{};
	bool srgbTexture = false;
};

class GammaRamp final : public glc::App {
public:
	GammaRamp()
	    : glc::App({
	          // The book's window is 500 wide; the extra room on the right is
	          // for the panel, so it does not cover the ramps.
	          .window = {.width = 800, .height = 192, .title = "gltut 16 -- Gamma Ramp"},
	          .depthTest = false,
	          .showStats = false, // it would sit on top of the upper ramp
	          .clearColor = {0.0F, 0.5F, 0.3F, 0.0F},
	      }) {}

protected:
	void onInit() override {
		noGammaProgram_ = &shaders().add(glc::paths::tutorialShader("screen_coords.vert"),
		                                 glc::paths::tutorialShader("texture_no_gamma.frag"));
		gammaProgram_ = &shaders().add(glc::paths::tutorialShader("screen_coords.vert"),
		                               glc::paths::tutorialShader("texture_gamma.frag"));
		configurePrograms();

		projectionBlock_.bindToPoint(kProjectionBlockBinding);

		vbo_ = glc::makeBuffer(GL_ARRAY_BUFFER, kVertices);
		vao_ = glc::makeVertexArray(vbo_, std::array{
		                                      glc::AttributeDesc{
		                                          .location = kPositionLocation,
		                                          .components = 2,
		                                          .type = GL_UNSIGNED_SHORT,
		                                          .stride = sizeof(RampVertex),
		                                          .offset = offsetof(RampVertex, x),
		                                      },
		                                      glc::AttributeDesc{
		                                          .location = kTexCoordLocation,
		                                          .components = 2,
		                                          .type = GL_UNSIGNED_SHORT,
		                                          .normalized = true,
		                                          .stride = sizeof(RampVertex),
		                                          .offset = offsetof(RampVertex, s),
		                                      },
		                                  });

		// The same bytes twice. The internal format is the only difference,
		// and it is the whole difference: GL_SRGB8 tells GL these bytes are
		// gamma-encoded, so texture() decodes them to linear on the way out.
		// GL_RGB8 says they are linear already and hands them back unchanged.
		//
		// The encoding is a property of the *data*, so it belongs with the
		// upload rather than in a shader -- and a texture decoded before
		// filtering averages linear values, which a shader-side pow() after
		// the fetch cannot do.
		const std::array<GLubyte, kBandCount * 3> pixels = rampPixels();
		const glc::Texture2DOptions noMipmaps{.generateMipmaps = false};
		linearTexture_ = glc::makeTexture2D(kBandCount, 1, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE,
		                                    pixels.data(), noMipmaps);
		srgbTexture_ = glc::makeTexture2D(kBandCount, 1, GL_SRGB8, GL_RGB, GL_UNSIGNED_BYTE,
		                                  pixels.data(), noMipmaps);

		// Nearest, so each band is one flat colour with a hard edge, as in the
		// book's image. Linear filtering would blend neighbouring bands into a
		// gradient -- and blend them differently for the two formats, which is
		// a different lesson (the next tutorial's).
		sampler_ = glc::makeSampler({
		    .minFilter = GL_NEAREST,
		    .magFilter = GL_NEAREST,
		    .wrapS = GL_CLAMP_TO_EDGE,
		    .wrapT = GL_CLAMP_TO_EDGE,
		});
	}

	void onResize(int /*width*/, int /*height*/) override {
		// Pixels from the top-left corner, Y down: scale to [0, 2] and flip,
		// then shift to [-1, 1]. Window size rather than framebuffer size --
		// on a Retina display the framebuffer is twice as large, and the quads
		// are laid out in the book's units, which are points.
		const glm::vec2 size{window().size()};
		glm::mat4 screenToClip = glm::translate(glm::mat4{1.0F}, glm::vec3{-1.0F, 1.0F, 0.0F});
		screenToClip = glm::scale(screenToClip, glm::vec3{2.0F / size.x, -2.0F / size.y, 1.0F});
		projectionBlock_.update(ProjectionBlock{screenToClip});
	}

	void onUpdate(float /*deltaSeconds*/) override {
		if (shaders().reloadCount() != appliedReloads_) {
			configurePrograms();
		}

		for (Row& row : rows_) {
			if (input().keyPressed(row.key)) {
				row.srgbTexture = !row.srgbTexture;
			}
		}
	}

	void onGui() override {
		ImGui::SetNextWindowPos(ImVec2{430.0F, 8.0F}, ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Tutorial 16")) {
			for (Row& row : rows_) {
				drawRowControls(row);
			}
			ImGui::Separator();
			ImGui::TextDisabled("Bytes: 0 135 186 223 255");
			ImGui::TextDisabled("As sRGB: 0 0.25 0.5 0.75 1");
		}
		ImGui::End();
	}

	void onRender() override {
		const glc::ScopedBind bind{vao_};

		drawRow(rows_[0], noGammaProgram_->get(), 0);
		drawRow(rows_[1], gammaProgram_->get(), kQuadVertexCount);
	}

private:
	glc::ReloadableProgram* noGammaProgram_{};
	glc::ReloadableProgram* gammaProgram_{};
	glc::UniformBuffer projectionBlock_{glc::UniformBuffer::forType<ProjectionBlock>()};

	glc::Buffer vbo_;
	glc::VertexArray vao_;

	glc::Texture linearTexture_;
	glc::Texture srgbTexture_;
	glc::Sampler sampler_;

	std::array<Row, 2> rows_{
	    {
	        {.label = "Top", .shader = "no gamma correction", .key = GLFW_KEY_1},
	        {.label = "Bottom", .shader = "pow(color, 1/2.2)", .key = GLFW_KEY_2},
	    },
	};
	int appliedReloads_{0};

	void drawRow(const Row& row, const glc::Program& program, GLint first) {
		program.use();
		glc::bindTextureUnit(kGammaRampTextureUnit, row.srgbTexture ? srgbTexture_ : linearTexture_,
		                     sampler_);
		GLC_CHECK(glDrawArrays(GL_TRIANGLE_STRIP, first, kQuadVertexCount));
	}

	void drawRowControls(Row& row) {
		ImGui::PushID(row.label);
		ImGui::Text("%-6s", row.label);
		ImGui::SameLine();
		if (ImGui::RadioButton("GL_RGB8", !row.srgbTexture)) {
			row.srgbTexture = false;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("GL_SRGB8", row.srgbTexture)) {
			row.srgbTexture = true;
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(%c)", row.key == GLFW_KEY_1 ? '1' : '2');
		ImGui::TextDisabled("  shader: %s", row.shader);
		ImGui::PopID();
	}

	/// Uniform block binding and sampler unit -- both reset by a relink.
	void configurePrograms() {
		for (const glc::ReloadableProgram* reloadable : {noGammaProgram_, gammaProgram_}) {
			const glc::Program& program = reloadable->get();
			program.bindUniformBlock("Projection", kProjectionBlockBinding);
			program.use();
			program.set("colorTexture", static_cast<int>(kGammaRampTextureUnit));
		}
		glc::Program::unuse();
		appliedReloads_ = shaders().reloadCount();
	}
};

} // namespace

int main() {
	return glc::runApp<GammaRamp>();
}
