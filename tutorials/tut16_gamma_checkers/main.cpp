// Tutorial 16 (Gamma and Textures): Gamma Checkers -- sRGB and Mipmaps.
//
// Tutorial 15's plane and corridor again, with two checkerboards that differ
// only in how their mipmaps were made. Both are loaded as GL_SRGB8, so every
// fetch is decoded to linear; the question is what the smaller levels hold.
//
// A mipmap texel stands for the average of the four above it. Averaging a
// black and a white texel should give the colour that *looks* halfway, and on
// a gamma-encoded image that is not the byte halfway between 0 and 255: half
// the light is linear 0.5, which encodes to 186, not 128. A tool that
// averages the stored bytes directly -- as most did -- builds levels that are
// too dark, and the further away the floor, the smaller the level and the
// darker the grey. That is checker_linear.dds. checker_gamma.dds had its
// levels built properly: decoded, averaged, re-encoded.
//
// None of that shows unless the output is gamma-corrected too, which is the
// other half of the toggle. With the plain shader the decoded linear values
// go to the screen as if they were encoded, the whole floor is too dark, and
// the badly-made mipmaps happen to look about right. With the gamma shader
// the near squares are restored -- and the distance shows which chain was made
// correctly: one fades to an even grey, the other to a band of darkness.
//
// Keys: A toggles the gamma shader, G the gamma-correct mipmaps, Space both at
// once (the book's "everything right" switch). 1 and 2 pick trilinear or
// maximum anisotropic filtering. Y swaps in the corridor; P pauses the camera.

#include <glcore/app.hpp>
#include <glcore/cycle_timer.hpp>
#include <glcore/mesh.hpp>
#include <glcore/paths.hpp>
#include <glcore/texture.hpp>
#include <glcore/uniform_buffer.hpp>

#include <glm/gtc/constants.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <imgui.h>

#include <array>
#include <cmath>
#include <cstddef>

namespace {

constexpr GLuint kProjectionBlockBinding = 0;
constexpr GLuint kColorTextureUnit = 0;

/// The scene and camera are Tutorial 15's, unchanged.
constexpr float kFovYDegrees = 90.0F;
constexpr float kZNear = 1.0F;
constexpr float kZFar = 1000.0F;

constexpr glm::vec3 kCameraEye{0.0F, 1.0F, -64.0F};
constexpr glm::vec3 kCameraTarget{0.0F, -5.0F, -44.0F};
constexpr float kCameraCycleSeconds = 5.0F;
constexpr float kCameraBobRadius = 0.25F;

struct ProjectionBlock {
	glm::mat4 cameraToClipMatrix;
};
static_assert(sizeof(ProjectionBlock) == 64);

/// The book keeps two of Tutorial 15's six samplers: the blur of trilinear
/// filtering is where badly-averaged mipmaps show most, and anisotropy is
/// there to show that sharper sampling does not hide them.
struct SamplerPreset {
	const char* label;
	bool anisotropic;
};

constexpr std::size_t kSamplerCount = 2;
constexpr std::array<SamplerPreset, kSamplerCount> kSamplers{
    {
        {.label = "Linear, linear mipmap", .anisotropic = false},
        {.label = "Max anisotropic", .anisotropic = true},
    },
};

class GammaCheckers final : public glc::App {
public:
	GammaCheckers()
	    : glc::App({
	          .window = {.title = "gltut 16 -- Gamma Checkers"},
	          .depthTest = true,
	          .cullFace = true,
	          .frontFace = GL_CW, // gltut's meshes are wound clockwise
	          .clearColor = {0.75F, 0.75F, 1.0F, 1.0F},
	      }) {}

protected:
	void onInit() override {
		planeMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/BigPlane.xml"));
		corridorMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/Corridor.xml"));

		noGammaProgram_ = &shaders().add(glc::paths::tutorialShader("textured.vert"),
		                                 glc::paths::tutorialShader("texture_no_gamma.frag"));
		gammaProgram_ = &shaders().add(glc::paths::tutorialShader("textured.vert"),
		                               glc::paths::tutorialShader("texture_gamma.frag"));
		configurePrograms();

		projectionBlock_.bindToPoint(kProjectionBlockBinding);

		// The files hold BGRA8 bytes and nothing to say whether they are
		// encoded, so both are uploaded as GL_SRGB8 by hand -- the book does
		// the same. Each carries its own full mip chain, which is the point:
		// generating one here would replace the difference being shown with
		// whatever the driver does.
		const glc::Texture2DOptions srgb{.internalFormat = GL_SRGB8};
		linearMipsTexture_ =
		    glc::loadTexture2D(glc::paths::asset("textures/checker_linear.dds"), srgb);
		gammaMipsTexture_ =
		    glc::loadTexture2D(glc::paths::asset("textures/checker_gamma.dds"), srgb);

		// Decoding happens per texel, before filtering, so the trilinear blend
		// between levels and the anisotropic taps are averages of linear
		// values. That is the other reason sRGB belongs in the texture format:
		// pow() applied after texture() would average encoded values first.
		const float maxAnisotropy = glc::maxSupportedAnisotropy();
		for (std::size_t i = 0; i < kSamplerCount; ++i) {
			samplers_[i] = glc::makeSampler({
			    .minFilter = GL_LINEAR_MIPMAP_LINEAR,
			    .magFilter = GL_LINEAR,
			    .wrapS = GL_REPEAT,
			    .wrapT = GL_REPEAT,
			    .maxAnisotropy = kSamplers[i].anisotropic ? maxAnisotropy : 1.0F,
			});
		}

		camera().setPerspective(kFovYDegrees, aspect(), kZNear, kZFar);
		projectionBlock_.update(ProjectionBlock{camera().projection()});

		// As in Tutorial 15: the ground under the camera is nearer than the
		// near plane, and clamping keeps it on screen.
		GLC_CHECK(glEnable(GL_DEPTH_CLAMP));
	}

	void onResize(int /*width*/, int /*height*/) override {
		projectionBlock_.update(ProjectionBlock{camera().projection()});
	}

	void onUpdate(float deltaSeconds) override {
		if (shaders().reloadCount() != appliedReloads_) {
			configurePrograms();
		}

		cameraTimer_.update(deltaSeconds);
		updateCamera();

		if (input().keyPressed(GLFW_KEY_A)) {
			gammaOutput_ = !gammaOutput_;
		}
		if (input().keyPressed(GLFW_KEY_G)) {
			gammaMipmaps_ = !gammaMipmaps_;
		}
		if (input().keyPressed(GLFW_KEY_SPACE)) {
			// Flips both, as the book does -- so from the default it switches
			// straight between all-wrong and all-right, and from a mixed state
			// to the opposite mix.
			gammaOutput_ = !gammaOutput_;
			gammaMipmaps_ = !gammaMipmaps_;
		}
		if (input().keyPressed(GLFW_KEY_Y)) {
			drawCorridor_ = !drawCorridor_;
		}
		if (input().keyPressed(GLFW_KEY_P)) {
			cameraTimer_.togglePause();
		}
		for (std::size_t i = 0; i < kSamplerCount; ++i) {
			if (input().keyPressed(GLFW_KEY_1 + static_cast<int>(i))) {
				currentSampler_ = i;
			}
		}
	}

	void onGui() override {
		if (ImGui::Begin("Tutorial 16")) {
			if (ImGui::CollapsingHeader("Gamma", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawGammaControls();
			}
			if (ImGui::CollapsingHeader("Sampler", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawSamplerControls();
			}
			if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawSceneControls();
			}
		}
		ImGui::End();
	}

	void onRender() override {
		const glc::Program& program = gammaOutput_ ? gammaProgram_->get() : noGammaProgram_->get();
		program.use();
		program.set("modelToCameraMatrix", camera().view());

		const glc::Texture& texture = gammaMipmaps_ ? gammaMipsTexture_ : linearMipsTexture_;
		glc::bindTextureUnit(kColorTextureUnit, texture, samplers_[currentSampler_]);

		const glc::Mesh& mesh = drawCorridor_ ? corridorMesh_ : planeMesh_;
		mesh.render("tex");
	}

private:
	glc::CycleTimer cameraTimer_{kCameraCycleSeconds};

	glc::ReloadableProgram* noGammaProgram_{};
	glc::ReloadableProgram* gammaProgram_{};
	glc::UniformBuffer projectionBlock_{glc::UniformBuffer::forType<ProjectionBlock>()};

	glc::Mesh planeMesh_;
	glc::Mesh corridorMesh_;

	glc::Texture linearMipsTexture_;
	glc::Texture gammaMipsTexture_;
	std::array<glc::Sampler, kSamplerCount> samplers_{};

	std::size_t currentSampler_{0};
	bool gammaOutput_{false};
	bool gammaMipmaps_{false};
	bool drawCorridor_{false};
	int appliedReloads_{0};

	void configurePrograms() {
		for (const glc::ReloadableProgram* reloadable : {noGammaProgram_, gammaProgram_}) {
			const glc::Program& program = reloadable->get();
			program.bindUniformBlock("Projection", kProjectionBlockBinding);
			program.use();
			program.set("colorTexture", static_cast<int>(kColorTextureUnit));
		}
		glc::Program::unuse();
		appliedReloads_ = shaders().reloadCount();
	}

	void updateCamera() {
		const float angle = cameraTimer_.alpha() * glm::two_pi<float>();
		const float slide = std::cos(angle) * kCameraBobRadius;
		const float nod = std::sin(angle) * kCameraBobRadius;

		camera().setView(kCameraEye + glm::vec3{slide, 0.0F, 0.0F},
		                 kCameraTarget + glm::vec3{slide, nod, 0.0F});
	}

	void drawGammaControls() {
		ImGui::Checkbox("Gamma-correct output", &gammaOutput_);
		ImGui::SameLine();
		ImGui::TextDisabled("(A)");

		ImGui::Checkbox("Gamma-correct mipmaps", &gammaMipmaps_);
		ImGui::SameLine();
		ImGui::TextDisabled("(G)");

		ImGui::TextDisabled("Space flips both.");
		ImGui::TextDisabled("Rendering: %s", gammaOutput_ ? "gamma" : "linear");
		ImGui::TextDisabled("Mipmaps:   %s",
		                    gammaMipmaps_ ? "checker_gamma.dds" : "checker_linear.dds");
	}

	void drawSamplerControls() {
		for (std::size_t i = 0; i < kSamplerCount; ++i) {
			if (ImGui::RadioButton(kSamplers[i].label, currentSampler_ == i)) {
				currentSampler_ = i;
			}
			ImGui::SameLine();
			ImGui::TextDisabled("(%zu)", i + 1);
		}
	}

	void drawSceneControls() {
		if (ImGui::RadioButton("Plane", !drawCorridor_)) {
			drawCorridor_ = false;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Corridor", drawCorridor_)) {
			drawCorridor_ = true;
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(Y)");

		bool paused = cameraTimer_.isPaused();
		if (ImGui::Checkbox("Camera paused", &paused)) {
			cameraTimer_.setPaused(paused);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(P)");
	}
};

} // namespace

int main() {
	return glc::runApp<GammaCheckers>();
}
