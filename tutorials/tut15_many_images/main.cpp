// Tutorial 15 (Many Images): Playing Checkers, Linear Filtering.
//
// The previous chapter's textures were tables. This one's is a picture, and
// the fragment shader does the least it can with it: fetch a texel, write it
// out. Everything interesting happens in the fetch -- which is why the scene
// is built to make the fetch hard.
//
// A flat plane 128 units across carries a 128x128 black-and-white
// checkerboard, and the camera stands one unit above it at one edge, looking
// across it at the horizon. The plane's texture coordinates run from -64 to
// 64 rather than 0 to 1, so the sampler's wrap mode -- GL_REPEAT on both axes
// -- tiles the picture 128 times in each direction. Near the camera each
// texel covers many pixels; at the horizon each pixel covers many texels, and
// with the nearest-texel filter the far half of the plane breaks up into
// shimmering moire. That is the book's Figure 15.1, and the rest of the
// chapter is about fixing it, one sampler at a time. Two are here: nearest
// (1), and linear (2), which fixes the near half -- the edges of the big
// squares at the bottom of the window stop crawling as the camera moves.
// The far half is untouched, and mipmaps and anisotropy are not here yet.
//
// The camera drifts on a small loop by itself; P pauses it. Y swaps the plane
// for a long square corridor, which puts the same texture on walls that
// recede at a steeper angle. The number keys pick a sampler.

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

/// The one image unit in use. The sampler uniform names it; the texture and
/// the sampler object are bound to it.
constexpr GLuint kColorTextureUnit = 0;

/// The book's 90 degree field of view, wide enough to take in the plane from
/// its edge. Near and far are the book's too; the plane's far edge is 128
/// units off, well inside.
constexpr float kFovYDegrees = 90.0F;
constexpr float kZNear = 1.0F;
constexpr float kZFar = 1000.0F;

/// The camera stands a unit above the plane at its near edge (z = -64) and
/// looks across it, tilted down enough to see the ground it is standing on.
constexpr glm::vec3 kCameraEye{0.0F, 1.0F, -64.0F};
constexpr glm::vec3 kCameraTarget{0.0F, -5.0F, -44.0F};

/// The camera is not still: it slides sideways and nods on a small circle,
/// once per cycle. A moving camera is what makes nearest filtering shimmer;
/// a still one only shows the moire.
constexpr float kCameraCycleSeconds = 5.0F;
constexpr float kCameraBobRadius = 0.25F;

/// The book's checker.dds: 128x128, black and white squares 16 texels on a
/// side, with a full mip chain that nothing reads yet. (The text says 32; the
/// file says 16.) It is kept under its own name because tut03's checker.png
/// is a different image.
constexpr int kCheckerTextureSize = 128;

struct ProjectionBlock {
	glm::mat4 cameraToClipMatrix;
};
static_assert(sizeof(ProjectionBlock) == 64);

/// One way of reading the texture. The book has six of these, on the keys 1
/// to 6; the rest arrive with the sections that explain them. Only the
/// filters vary -- the wrap modes are the same for all of them, and set where
/// the samplers are made.
///
/// A filter answers one question: what does a coordinate that lands between
/// texels return? Magnification and minification are set separately, because
/// a texture drawn larger than its resolution and one drawn smaller have
/// different problems; these first two make the same choice for both.
struct SamplerPreset {
	const char* label;
	GLenum minFilter;
	GLenum magFilter;
};

constexpr std::size_t kSamplerCount = 2;

constexpr std::array<SamplerPreset, kSamplerCount> kSamplers{
    {
        // The texel the coordinate is nearest to, and nothing else. A
        // coordinate halfway between black and white is one or the other, so
        // a small move of the camera flips it -- pixel crawl along every
        // edge (Figure 15.2).
        {.label = "Nearest", .minFilter = GL_NEAREST, .magFilter = GL_NEAREST},
        // The four texels around the coordinate, weighted by how near each
        // one is. A fragment covers an area of the texture, not a point, and
        // this is the cheapest stand-in for averaging that area: the edges
        // go a little soft and stop crawling (Figure 15.4).
        {.label = "Linear", .minFilter = GL_LINEAR, .magFilter = GL_LINEAR},
    },
};

/// Draws the texture into the panel at one screen pixel per texel, T = 0 at
/// the bottom as GL has it.
void drawTexturePreview(const glc::Texture& texture, int size) {
	const auto side = static_cast<float>(size);
	ImGui::Image(ImTextureRef{static_cast<ImTextureID>(texture.id())}, ImVec2{side, side},
	             ImVec2{0.0F, 1.0F}, ImVec2{1.0F, 0.0F});
}

class ManyImages final : public glc::App {
public:
	ManyImages()
	    : glc::App({
	          .window = {.title = "gltut 15 -- Many Images"},
	          .depthTest = true,
	          .cullFace = true,
	          .frontFace = GL_CW, // gltut's meshes are wound clockwise
	          .clearColor = {0.75F, 0.75F, 1.0F, 1.0F},
	      }) {}

protected:
	void onInit() override {
		// Both meshes carry a "tex" VAO with positions and texture coordinates,
		// and a "flat" one with positions alone that nothing here draws.
		planeMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/BigPlane.xml"));
		corridorMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/Corridor.xml"));

		program_ = &shaders().add(glc::paths::tutorialShader("textured.vert"),
		                          glc::paths::tutorialShader("textured.frag"));
		configureProgram();

		projectionBlock_.bindToPoint(kProjectionBlockBinding);

		// The file carries its mip chain, so the loader uploads eight levels
		// and generates nothing. Neither sampler here reads past level 0.
		checkerTexture_ = glc::loadTexture2D(glc::paths::asset("textures/checker_gltut.dds"));

		// The mesh's coordinates go far outside [0, 1], and GL_REPEAT is what
		// makes that mean "tile it": 1.1 reads the texel at 0.1, -0.1 the one
		// at 0.9, and the picture repeats as if it were infinitely large. The
		// two axes need not agree -- S could clamp while T repeats -- but here
		// every sampler wraps both.
		for (std::size_t i = 0; i < kSamplerCount; ++i) {
			samplers_[i] = glc::makeSampler({
			    .minFilter = kSamplers[i].minFilter,
			    .magFilter = kSamplers[i].magFilter,
			    .wrapS = GL_REPEAT,
			    .wrapT = GL_REPEAT,
			});
		}

		camera().setPerspective(kFovYDegrees, aspect(), kZNear, kZFar);
		projectionBlock_.update(ProjectionBlock{camera().projection()});

		// The eye is one unit above the ground and the near plane is one unit
		// out, so the strip of ground under the bottom of the view is closer
		// than the near plane and would be clipped, leaving a band of sky
		// below the checkerboard. Depth clamping turns near and far clipping
		// off and clamps the depth of what gets through instead; the book has
		// it on, and its Figure 15.1 reaches the bottom of the window.
		GLC_CHECK(glEnable(GL_DEPTH_CLAMP));
	}

	void onResize(int /*width*/, int /*height*/) override {
		projectionBlock_.update(ProjectionBlock{camera().projection()});
	}

	void onUpdate(float deltaSeconds) override {
		// A hot reload relinks the program, and a fresh link has the uniform
		// block on point 0 and the sampler on unit 0 -- which happen to be
		// right, but only by accident, so it is redone anyway.
		if (shaders().reloadCount() != appliedReloads_) {
			configureProgram();
		}

		cameraTimer_.update(deltaSeconds);
		updateCamera();

		// input() reports nothing while ImGui owns the keyboard.
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
		if (ImGui::Begin("Tutorial 15")) {
			if (ImGui::CollapsingHeader("Sampler", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawSamplerControls();
			}
			if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawSceneControls();
			}
			if (ImGui::CollapsingHeader("Texture")) {
				ImGui::Text("checker_gltut.dds, %d x %d", kCheckerTextureSize, kCheckerTextureSize);
				drawTexturePreview(checkerTexture_, kCheckerTextureSize);
				ImGui::TextDisabled("Tiled 128 times across the plane.");
			}
		}
		ImGui::End();
	}

	void onRender() override {
		const glc::Program& program = program_->get();
		program.use();

		// The meshes sit at the origin, so model-to-camera is the view alone.
		program.set("modelToCameraMatrix", camera().view());

		// Texture and sampler go on the same unit. The texture says what is
		// read; the sampler says how -- and swapping the sampler is all the
		// number keys do.
		glc::bindTextureUnit(kColorTextureUnit, checkerTexture_, samplers_[currentSampler_]);

		const glc::Mesh& mesh = drawCorridor_ ? corridorMesh_ : planeMesh_;
		mesh.render("tex");
	}

private:
	glc::CycleTimer cameraTimer_{kCameraCycleSeconds};

	glc::ReloadableProgram* program_{};
	glc::UniformBuffer projectionBlock_{glc::UniformBuffer::forType<ProjectionBlock>()};

	glc::Mesh planeMesh_;
	glc::Mesh corridorMesh_;

	glc::Texture checkerTexture_;
	std::array<glc::Sampler, kSamplerCount> samplers_{};

	std::size_t currentSampler_{0};
	bool drawCorridor_{false};
	int appliedReloads_{0};

	/// Everything that survives only until the next relink: the uniform block
	/// binding, and the image unit the sampler uniform reads from.
	void configureProgram() {
		const glc::Program& program = program_->get();
		program.bindUniformBlock("Projection", kProjectionBlockBinding);

		// A sampler uniform is set to a unit number, not a texture.
		program.use();
		program.set("colorTexture", static_cast<int>(kColorTextureUnit));
		glc::Program::unuse();

		appliedReloads_ = shaders().reloadCount();
	}

	/// The book's camera: a look-at from a fixed spot, with the eye sliding
	/// sideways and the target nodding as the timer goes round.
	void updateCamera() {
		const float angle = cameraTimer_.alpha() * glm::two_pi<float>();
		const float slide = std::cos(angle) * kCameraBobRadius;
		const float nod = std::sin(angle) * kCameraBobRadius;

		camera().setView(kCameraEye + glm::vec3{slide, 0.0F, 0.0F},
		                 kCameraTarget + glm::vec3{slide, nod, 0.0F});
	}

	void drawSamplerControls() {
		for (std::size_t i = 0; i < kSamplerCount; ++i) {
			if (ImGui::RadioButton(kSamplers[i].label, currentSampler_ == i)) {
				currentSampler_ = i;
			}
			ImGui::SameLine();
			ImGui::TextDisabled("(%zu)", i + 1);
		}
		ImGui::TextDisabled("Wrap S and T: GL_REPEAT, for every sampler.");
		ImGui::TextDisabled("Watch the bottom edge of the window while the camera moves.");
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

		// Seeded from the timer every frame so the slider tracks it and
		// becomes a scrub handle the moment it is touched.
		bool paused = cameraTimer_.isPaused();
		if (ImGui::Checkbox("Camera paused", &paused)) {
			cameraTimer_.setPaused(paused);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(P)");

		float alpha = cameraTimer_.alpha();
		if (ImGui::SliderFloat("Camera cycle", &alpha, 0.0F, 1.0F, "%.2f")) {
			cameraTimer_.setAlpha(alpha);
		}
	}
};

} // namespace

int main() {
	return glc::runApp<ManyImages>();
}
