// Tutorial 15 (Many Images): Playing Checkers, Linear Filtering, Needs More
// Pictures, Anisotropy.
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
// chapter is about fixing it, one sampler at a time.
//
// Six are here. Nearest (1) and linear (2) differ only in what a coordinate
// between texels returns, and both sample the full-size image: linear fixes
// the near half of the plane, where a texel covers many pixels, and does
// nothing for the far half, where the problem is the opposite. Fixing that
// end needs smaller copies of the image to sample instead -- mipmaps -- and
// a minification filter that says to use them: GL_LINEAR_MIPMAP_NEAREST (3)
// picks the one nearest the fragment's size, GL_LINEAR_MIPMAP_LINEAR (4)
// blends the two nearest, which hides the seam where one level gives way to
// the next.
//
// What is still wrong after that is the blur: the far half of the plane goes
// grey well before the squares are too small to see. A fragment on a floor
// seen edge-on covers a long thin sliver of texture -- many texels deep, few
// wide -- and mipmapping picks its level by the long side, so it throws away
// detail across the short one. Anisotropic filtering (5 and 6) samples along
// the long side instead, several times, from a level chosen for the short
// one. 5 allows up to four samples, 6 as many as the driver will take.
//
// Spacebar swaps the checkerboard for a texture whose every mipmap level is a
// different flat colour, so the levels themselves are visible: each band of
// colour on the floor is one mipmap, and under filter 4 the bands fade into
// each other instead of meeting at a line.
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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

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
/// to 6. Only the filters vary -- the wrap modes are the same for all of them, and set where
/// the samplers are made.
///
/// A filter answers one question: what does a coordinate that lands between
/// texels return? Magnification and minification are set separately, because
/// a texture drawn larger than its resolution and one drawn smaller have
/// different problems. Only minification can name a mipmap mode -- a fragment
/// smaller than a texel has no use for a smaller copy of the image -- which is
/// why the mipmap presets differ from the second in the min filter alone.
///
/// Anisotropy is not a filter mode but a limit on top of one: how many samples
/// the filter may take along a fragment's footprint. 1 means just the one.
struct SamplerPreset {
	const char* label{};
	GLenum minFilter{};
	GLenum magFilter{};
	float maxAnisotropy = 1.0F;
};

/// Stands in for the driver's own limit, which is not known until there is a
/// context to ask. makeSampler clamps anything above the limit down to it, but
/// the panel wants to say what the limit is, so it is looked up by name.
constexpr float kDriverMaxAnisotropy = 0.0F;

constexpr std::size_t kSamplerCount = 6;

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
        // Four texels is still four texels, and at the horizon the fragment
        // covers far more than that -- which is what the distant mess is.
        // This picks the mipmap level whose texels are closest to the size
        // of the fragment and takes its four texels instead, so the value
        // comes back a plausible grey rather than whichever two of black and
        // white happened to be sampled (Figure 15.7).
        {
            .label = "Linear, nearest mipmap",
            .minFilter = GL_LINEAR_MIPMAP_NEAREST,
            .magFilter = GL_LINEAR,
        },
        // The same, but sampling the two levels either side of the fragment's
        // size and blending between them. One level is chosen per fragment
        // above, so neighbouring fragments can land on different levels and
        // the change shows up as a visible line across the floor; blending
        // smears that line out (Figure 15.9). The two LINEARs are separate
        // choices: within a level, and between levels.
        {
            .label = "Linear, linear mipmap",
            .minFilter = GL_LINEAR_MIPMAP_LINEAR,
            .magFilter = GL_LINEAR,
        },
        // The fragment's footprint on the floor is a thin sliver pointing
        // away from the camera, and the level above was picked to fit its
        // long side -- too coarse for the short one, hence the blur. This
        // picks a level for the short side and takes up to four samples down
        // the long one (Figure 15.13). The mipmap filter still applies: each
        // of those samples is itself a trilinear fetch.
        {
            .label = "Low anisotropic",
            .minFilter = GL_LINEAR_MIPMAP_LINEAR,
            .magFilter = GL_LINEAR,
            .maxAnisotropy = 4.0F,
        },
        // The same, with no cap but the hardware's -- 16 on most of it. The
        // difference from 4 is only at the horizon, where the sliver is
        // longest (Figure 15.14).
        {
            .label = "Max anisotropic",
            .minFilter = GL_LINEAR_MIPMAP_LINEAR,
            .magFilter = GL_LINEAR,
            .maxAnisotropy = kDriverMaxAnisotropy,
        },
    },
};

/// The book's mipmapColors. One flat colour per level of the special texture,
/// which is not a picture of anything -- it exists so that the mipmap level a
/// fragment ends up on is visible on screen.
constexpr std::size_t kMipmapLevelCount = 8;

constexpr std::array<std::array<GLubyte, 3>, kMipmapLevelCount> kMipmapColors{
    {
        {0xFF, 0xFF, 0x00}, // level 0, the full 128x128
        {0xFF, 0x00, 0xFF},
        {0x00, 0xFF, 0xFF},
        {0xFF, 0x00, 0x00},
        {0x00, 0xFF, 0x00},
        {0x00, 0x00, 0xFF},
        {0x00, 0x00, 0x00},
        {0xFF, 0xFF, 0xFF}, // level 7, a single texel
    },
};

/// The special texture: eight levels, each a single colour.
///
/// Ordinary mipmaps are smaller versions of one image, and the level a
/// fragment lands on is invisible precisely because every level looks alike.
/// Making them disagree is what turns level selection into something you can
/// watch, and nothing says a chain has to be self-consistent -- GL only cares
/// that the sizes halve.
///
/// These texels are three bytes, not four, which is the chapter's aside about
/// alignment: the 2x2 level's rows are 6 bytes long and GL's default unpack
/// alignment of 4 would misread them. makeTexture2D sets the alignment to 1
/// for exactly this reason, so nothing here has to.
[[nodiscard]] glc::Texture makeMipmapTestTexture() {
	// The spans handed over have to stay alive until the upload, so every
	// level is built before any of them is described.
	std::array<std::vector<GLubyte>, kMipmapLevelCount> pixels;
	std::array<glc::MipLevel, kMipmapLevelCount> levels{};

	GLsizei side = kCheckerTextureSize;
	for (std::size_t level = 0; level < kMipmapLevelCount; ++level) {
		const auto texels = static_cast<std::size_t>(side) * static_cast<std::size_t>(side);
		const std::array<GLubyte, 3>& color = kMipmapColors[level];

		std::vector<GLubyte>& buffer = pixels[level];
		buffer.reserve(texels * color.size());
		for (std::size_t texel = 0; texel < texels; ++texel) {
			buffer.insert(buffer.end(), color.begin(), color.end());
		}

		levels[level] = {.width = side, .height = side, .pixels = buffer.data()};
		side /= 2; // every level is half the last, down to 1x1
	}

	// GL_RGB8 from GL_RGB bytes. The checkerboard beside it is GL_RGB8 from
	// BGRA bytes in a file: the internal format is what GL keeps, and it need
	// not resemble the layout handed over.
	return glc::makeTexture2D(levels, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE);
}

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

		// The file carries its mip chain, so the loader uploads all eight
		// levels and generates nothing. Those levels have been there since the
		// first sampler was written; what changed is that two of the samplers
		// now ask for them. DDS is one of the few formats that can hold a
		// whole chain in one file, which is why the book ships this as DDS.
		checkerTexture_ = glc::loadTexture2D(glc::paths::asset("textures/checker_gltut.dds"));
		mipmapTestTexture_ = makeMipmapTestTexture();

		// The mesh's coordinates go far outside [0, 1], and GL_REPEAT is what
		// makes that mean "tile it": 1.1 reads the texel at 0.1, -0.1 the one
		// at 0.9, and the picture repeats as if it were infinitely large. The
		// two axes need not agree -- S could clamp while T repeats -- but here
		// every sampler wraps both.
		//
		// Anisotropic filtering is still an extension --
		// EXT_texture_filter_anisotropic, core only from GL 4.6 -- though one
		// every desktop driver has. Without it the limit comes back as 1 and
		// the last two samplers are the fourth under other names.
		driverMaxAnisotropy_ = glc::maxSupportedAnisotropy();
		for (std::size_t i = 0; i < kSamplerCount; ++i) {
			samplers_[i] = glc::makeSampler({
			    .minFilter = kSamplers[i].minFilter,
			    .magFilter = kSamplers[i].magFilter,
			    .wrapS = GL_REPEAT,
			    .wrapT = GL_REPEAT,
			    .maxAnisotropy = anisotropyOf(kSamplers[i]),
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
		if (input().keyPressed(GLFW_KEY_SPACE)) {
			useMipmapTexture_ = !useMipmapTexture_;
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
			if (ImGui::CollapsingHeader("Texture", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawTextureControls();
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
		const glc::Texture& texture = useMipmapTexture_ ? mipmapTestTexture_ : checkerTexture_;
		glc::bindTextureUnit(kColorTextureUnit, texture, samplers_[currentSampler_]);

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
	glc::Texture mipmapTestTexture_;
	std::array<glc::Sampler, kSamplerCount> samplers_{};

	float driverMaxAnisotropy_{1.0F};
	std::size_t currentSampler_{0};
	bool drawCorridor_{false};
	bool useMipmapTexture_{false};
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

	/// The anisotropy a preset asks for, with the driver's limit filled in.
	[[nodiscard]] float anisotropyOf(const SamplerPreset& preset) const {
		return preset.maxAnisotropy == kDriverMaxAnisotropy ? driverMaxAnisotropy_
		                                                    : preset.maxAnisotropy;
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
		ImGui::TextDisabled("1 vs 2: the bottom edge. 2 vs 3: the horizon.");
		ImGui::TextDisabled("4 vs 5: the middle distance. 5 vs 6: the horizon.");

		if (driverMaxAnisotropy_ > 1.0F) {
			// What the sampler actually got: makeSampler clamps to the limit.
			const float applied =
			    std::min(anisotropyOf(kSamplers[currentSampler_]), driverMaxAnisotropy_);
			ImGui::Text("Max anisotropy: %.0f (driver limit %.0f)", static_cast<double>(applied),
			            static_cast<double>(driverMaxAnisotropy_));
		} else {
			ImGui::TextDisabled("EXT_texture_filter_anisotropic is unavailable.");
		}
	}

	void drawTextureControls() {
		if (ImGui::RadioButton("Checkerboard", !useMipmapTexture_)) {
			useMipmapTexture_ = false;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Mipmap levels", useMipmapTexture_)) {
			useMipmapTexture_ = true;
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(Space)");

		if (useMipmapTexture_) {
			ImGui::Text("Built in code, %d x %d, %zu levels", kCheckerTextureSize,
			            kCheckerTextureSize, kMipmapLevelCount);
			drawTexturePreview(mipmapTestTexture_, kCheckerTextureSize);
			ImGui::TextDisabled("Level 0 is yellow; the preview shows only it.");
		} else {
			ImGui::Text("checker_gltut.dds, %d x %d, %zu levels", kCheckerTextureSize,
			            kCheckerTextureSize, kMipmapLevelCount);
			drawTexturePreview(checkerTexture_, kCheckerTextureSize);
			ImGui::TextDisabled("Tiled 128 times across the plane.");
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
