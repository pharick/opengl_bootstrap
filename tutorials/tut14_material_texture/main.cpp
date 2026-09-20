// Tutorial 14 (Textures are not Pictures): the Material Texture example.
//
// Basic Texture tabulated the Gaussian specular term over its one free
// argument, because the shininess was a per-material constant. This example
// lets the shininess vary across the surface: it comes out of a second
// texture, mapped onto the mesh by per-vertex texture coordinates. That makes
// the specular term a function of two variables again, so the table grows a
// second axis -- S is the cosine, T is the shininess -- and becomes a 2D
// texture.
//
// The scene is the same golden infinity symbol under a sun and an orbiting
// lamp. Spacebar cycles three ways of shading it: the material's one shininess
// looked up in the 2D table; the texture's shininess looked up in the table;
// the texture's shininess with the Gaussian computed in the shader. Y swaps
// the infinity symbol for a flat plane, where the smudges are easier to read;
// 9 switches to a near-black material with a white highlight, which makes
// them impossible to miss, and 8 goes back to gold. 1-4 pick the table's
// resolution along S, as before.
//
// Two things happen here for the first time: a texture is loaded from a file
// (assets/textures/shininess.dds, the book's main.dds), and a mesh carries
// texture coordinates as a vertex attribute -- which is the one topological
// difference between the "lit" and "lit-tex" VAOs the modes draw through.

#include <glcore/app.hpp>
#include <glcore/cycle_timer.hpp>
#include <glcore/gl_check.hpp>
#include <glcore/mesh.hpp>
#include <glcore/paths.hpp>
#include <glcore/texture.hpp>
#include <glcore/uniform_buffer.hpp>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <imgui.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

constexpr GLuint kMaterialBlockBinding = 0;
constexpr GLuint kLightBlockBinding = 1;
constexpr GLuint kProjectionBlockBinding = 2;

/// Two textures, two image units. The sampler uniform in each program names
/// one of these; the texture bound there is what it reads.
constexpr GLuint kGaussTextureUnit = 0;
constexpr GLuint kShineTextureUnit = 1;

/// One orbit of the point light.
constexpr float kOrbitSeconds = 6.0F;

/// Seconds moved per press of a scrub button; the book's `-` and `=` keys.
constexpr float kScrubSeconds = 0.5F;

constexpr float kLightHeight = 1.0F;
constexpr float kLightOrbitRadius = 3.0F;

/// The lamp falls to half brightness 25 units out, under inverse-square falloff.
constexpr float kHalfLightDistance = 25.0F;
constexpr float kLightAttenuation = 1.0F / (kHalfLightDistance * kHalfLightDistance);

constexpr glm::vec3 kSunDirection{0.707F, 0.707F, 0.0F};

/// The sun's marker is a large cube parked a long way down its direction.
constexpr float kSunMarkerDistance = 100.0F;
constexpr float kSunMarkerScale = 5.0F;
constexpr float kLightMarkerScale = 0.25F;

/// The infinity symbol is drawn at 2x, the unit plane at 4x so it fills about
/// the same part of the view.
constexpr float kInfinityScale = 2.0F;
constexpr float kPlaneScale = 4.0F;

/// Right-drag spins the object; this is the book's ObjectPole rate.
constexpr float kObjectDegreesPerPixel = 90.0F / 250.0F;

/// The book fixes this for both materials. It is only read by the fixed
/// mode; the other two get their shininess from the texture.
constexpr float kDefaultShininess = 0.125F;
constexpr float kMinShininess = 0.02F;
constexpr float kMaxShininess = 1.0F;

/// The 1-4 keys pick the table's resolution along S, the cosine axis:
/// 64, 128, 256 or 512 columns. Every table has the same number of rows.
constexpr std::size_t kTableCount = 4;
constexpr std::size_t kSmallestTable = 64;

/// Rows along T, the shininess axis. 128 steps of 1/128 rad each.
constexpr GLsizei kShininessResolution = 128;

[[nodiscard]] constexpr GLsizei cosAngleResolution(std::size_t level) {
	return static_cast<GLsizei>(kSmallestTable << level);
}

/// The book's main.dds. Only the panel's preview needs to know the size.
constexpr int kShininessTextureWidth = 1024;
constexpr int kShininessTextureHeight = 256;

/// Preview height in the panel for the tables and the shininess texture.
constexpr float kPreviewHeight = 64.0F;

struct ProjectionBlock {
	glm::mat4 cameraToClipMatrix;
};
static_assert(sizeof(ProjectionBlock) == 64);

/// `cameraSpaceLightPos.w` selects the kind: 0 makes xyz a direction (the sun,
/// unattenuated), 1 makes it a position.
struct PerLight {
	glm::vec4 cameraSpaceLightPos;
	glm::vec4 intensity;
};

constexpr std::size_t kNumberOfLights = 2;

/// std140 layout. glm's vec4 aligns to 4, not 16, so the padding is manual and
/// the static_assert is the only thing checking it.
struct LightBlock {
	glm::vec4 ambientIntensity;
	float lightAttenuation;
	std::array<float, 3> padding; ///< pushes lights[0] to offset 32
	std::array<PerLight, kNumberOfLights> lights;
};
static_assert(sizeof(LightBlock) == 96);

struct MaterialBlock {
	glm::vec4 diffuseColor;
	glm::vec4 specularColor;
	float specularShininess; ///< Gaussian width in radians, not a Phong exponent
	std::array<float, 3> padding;
};
static_assert(sizeof(MaterialBlock) == 48);

/// The book's 8 and 9 keys. Gold shows the effect; the dark material shows
/// nothing *but* the effect -- there is no diffuse to speak of, so every
/// visible feature is the highlight, and the highlight is what the shininess
/// texture shapes.
enum class MaterialId : std::uint8_t {
	Gold = 0,
	DarkShiny,
};
constexpr std::size_t kMaterialCount = 2;

struct MaterialPreset {
	const char* name;
	int key; ///< the GLFW key that selects it
	glm::vec4 diffuseColor;
	glm::vec4 specularColor;
};

constexpr std::array<MaterialPreset, kMaterialCount> kMaterials{
    {
        {
            // Gold, with the specular tinted like the diffuse -- a metal.
            .name = "Gold",
            .key = GLFW_KEY_8,
            .diffuseColor = {1.0F, 0.673F, 0.043F, 1.0F},
            .specularColor = {1.0F * 0.4F, 0.673F * 0.4F, 0.043F * 0.4F, 1.0F},
        },
        {
            .name = "Dark, bright specular",
            .key = GLFW_KEY_9,
            .diffuseColor = {0.01F, 0.01F, 0.01F, 1.0F},
            .specularColor = {0.99F, 0.99F, 0.99F, 1.0F},
        },
    },
};

/// What the Spacebar cycles through, in the book's order.
enum class ShadingMode : std::uint8_t {
	FixedShininess = 0, ///< Mtl.specularShininess, looked up in the table
	TextureShininess,   ///< shininess texture, looked up in the table
	TextureCompute,     ///< shininess texture, Gaussian computed per fragment
};
constexpr std::size_t kModeCount = 3;

/// Everything that differs between the modes, so the render loop does not
/// have to know which one it is drawing.
struct ModeInfo {
	const char* label;
	const char* vertexShader;
	const char* fragmentShader;
	/// Which of the mesh's VAOs to draw through. Only "lit-tex" sources the
	/// texture coordinates; the fixed mode's vertex shader has no input for
	/// them, so it draws the mesh exactly as Tutorial 13 did.
	const char* vao;
	bool usesTable;
	bool usesShininessTexture;
};

constexpr std::array<ModeInfo, kModeCount> kModes{
    {
        {
            .label = "Fixed shininess, Gaussian from the table",
            .vertexShader = "mesh.vert",
            .fragmentShader = "fixed_shininess.frag",
            .vao = "lit",
            .usesTable = true,
            .usesShininessTexture = false,
        },
        {
            .label = "Textured shininess, Gaussian from the table",
            .vertexShader = "mesh_tex.vert",
            .fragmentShader = "texture_shininess.frag",
            .vao = "lit-tex",
            .usesTable = true,
            .usesShininessTexture = true,
        },
        {
            .label = "Textured shininess, Gaussian computed",
            .vertexShader = "mesh_tex.vert",
            .fragmentShader = "texture_compute.frag",
            .vao = "lit-tex",
            .usesTable = false,
            .usesShininessTexture = true,
        },
    },
};

[[nodiscard]] constexpr const ModeInfo& modeInfo(ShadingMode mode) {
	return kModes[static_cast<std::size_t>(mode)];
}

/// The Gaussian specular term, tabulated over both of its arguments.
///
/// One row per shininess value, each row a full sweep of the cosine, and the
/// rows laid out one after another -- the order OpenGL takes a 2D image in,
/// and the order almost every image format stores one in. The one thing to
/// notice is which row comes first: row 0 is the *smallest* shininess, so it
/// sits at T = 0, the bottom of the texture. Most image formats put their
/// first row at the top; a texture built in code can simply not do that.
///
/// The column at index i holds cos = i / (columns - 1), so both ends of the
/// cosine axis land exactly on 0 and 1. The rows are one step *up* from that:
/// row j holds shininess (j + 1) / rows, because a Gaussian of width 0 is
/// 0 / 0 at its own peak. So T does not quite read as the shininess -- it
/// rounds it up to the next 1/128 -- and nobody can tell.
[[nodiscard]] std::vector<GLubyte> buildGaussianTable(GLsizei columns, GLsizei rows) {
	const auto width = static_cast<std::size_t>(columns);
	const auto height = static_cast<std::size_t>(rows);
	std::vector<GLubyte> table(width * height);

	for (std::size_t row = 0; row < height; ++row) {
		const float shininess = static_cast<float>(row + 1) / static_cast<float>(height);
		for (std::size_t column = 0; column < width; ++column) {
			const float cosAngle = static_cast<float>(column) / static_cast<float>(width - 1);
			const float exponent = std::acos(cosAngle) / shininess;
			const float gaussian = std::exp(-(exponent * exponent));
			table[(row * width) + column] = static_cast<GLubyte>(gaussian * 255.0F);
		}
	}
	return table;
}

/// One table as a 2D texture: GL_R8 again, one normalized byte per texel, the
/// only change from the 1D version being the extra dimension. No mip chain;
/// a look-up table has no use for one.
///
/// The swizzle is for the panel's preview only. Reading `.r` in a shader is
/// unaffected by it -- red still maps to red -- but ImGui draws all four
/// channels, and a single-channel texture with the default swizzle comes out
/// pure red rather than gray.
[[nodiscard]] glc::Texture makeGaussianTexture(GLsizei columns, GLsizei rows) {
	const std::vector<GLubyte> table = buildGaussianTable(columns, rows);
	glc::Texture texture = glc::makeTexture2D(columns, rows, GL_R8, GL_RED, GL_UNSIGNED_BYTE,
	                                          table.data(), {.generateMipmaps = false});

	constexpr std::array<GLint, 4> kGrayscale{GL_RED, GL_RED, GL_RED, GL_ONE};
	glBindTexture(GL_TEXTURE_2D, texture.id());
	GLC_CHECK(glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, kGrayscale.data()));
	glBindTexture(GL_TEXTURE_2D, 0);

	return texture;
}

/// The book's main.dds: 1024x256, one 8-bit channel, no mip chain.
///
/// This is the first texture in the book that comes out of a file. The book
/// goes through its GL Image library to get at the bytes and then makes the
/// same glTexImage2D call as for the table, with GL_R8 / GL_RED /
/// GL_UNSIGNED_BYTE; loadTexture2D does the same through gli. DDS is used for
/// a reason worth knowing: unlike PNG, it stores the pixels in a form GL can
/// take directly, with the format written in the file. The one wrinkle is
/// that it is a luminance image, a format core GL no longer has; gli
/// reports it as GL_R8 with a swizzle that reads the red channel into all
/// three, which is exactly what a luminance texture did.
///
/// It is 4x wider than it is tall because it is drawn for the infinity
/// symbol, which is about that much longer along S than around T. The round
/// smudges in the file come out round on the symbol and stretched into ovals
/// on the square plane.
[[nodiscard]] glc::Texture loadShininessTexture() {
	return glc::loadTexture2D(glc::paths::asset("textures/shininess.dds"),
	                          {.generateMipmaps = false});
}

/// Folds an angle into [-180, 180) so a slider can show it after any amount
/// of dragging.
[[nodiscard]] float wrapDegrees(float degrees) {
	degrees = std::fmod(degrees + 180.0F, 360.0F);
	if (degrees < 0.0F) {
		degrees += 360.0F;
	}
	return degrees - 180.0F;
}

/// Draws a texture into the panel at kPreviewHeight, or narrower if the panel
/// is. With uv0 at the top-left set to T = 1, the preview keeps the texture's
/// own orientation: T = 0 at the bottom, which for the table means the
/// smallest shininess is the bottom row.
void drawTexturePreview(const glc::Texture& texture, float aspect) {
	float height = kPreviewHeight;
	float width = height * aspect;
	const float available = ImGui::GetContentRegionAvail().x;
	if (width > available) {
		width = available;
		height = width / aspect;
	}
	ImGui::Image(ImTextureRef{static_cast<ImTextureID>(texture.id())}, ImVec2{width, height},
	             ImVec2{0.0F, 1.0F}, ImVec2{1.0F, 0.0F});
}

class MaterialTexture final : public glc::App {
public:
	MaterialTexture()
	    : glc::App({
	          .window = {.title = "gltut 14 -- Material Texture"},
	          .depthTest = true,
	          .cullFace = true,
	          .frontFace = GL_CW, // gltut's meshes are wound clockwise
	          .clearColor = {0.75F, 0.75F, 1.0F, 1.0F},
	      }) {}

protected:
	void onInit() override {
		infinityMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/Infinity.xml"));
		planeMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitPlane.xml"));
		cubeMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitCubeLit.xml"));

		for (std::size_t i = 0; i < kModeCount; ++i) {
			programs_[i] = &shaders().add(glc::paths::tutorialShader(kModes[i].vertexShader),
			                              glc::paths::tutorialShader(kModes[i].fragmentShader));
		}
		unlitProgram_ = &shaders().add(glc::paths::tutorialShader("unlit.vert"),
		                               glc::paths::tutorialShader("unlit.frag"));
		configurePrograms();

		projectionBlock_.bindToPoint(kProjectionBlockBinding);
		lightBlock_.bindToPoint(kLightBlockBinding);
		materialBlock_.bindToPoint(kMaterialBlockBinding);
		uploadMaterial();

		// Built once. The shininess is no longer baked in, so nothing about
		// the material can invalidate these -- compare Basic Texture, which
		// had to rebuild all four whenever the slider moved.
		for (std::size_t i = 0; i < kTableCount; ++i) {
			gaussTextures_[i] = makeGaussianTexture(cosAngleResolution(i), kShininessResolution);
		}
		shininessTexture_ = loadShininessTexture();

		// One sampler object for both textures. Both want to be read the
		// same way -- nearest, so the texels show, and clamped, so a
		// coordinate off the edge lands on the edge -- and a sampler object
		// is not tied to a texture, so nothing says there must be two. Now
		// that the textures are 2D, T needs clamping as well as S.
		sampler_ = glc::makeSampler({
		    .minFilter = GL_NEAREST,
		    .magFilter = GL_NEAREST,
		    .wrapS = GL_CLAMP_TO_EDGE,
		    .wrapT = GL_CLAMP_TO_EDGE,
		});

		camera().setPerspective(45.0F, aspect(), 1.0F, 1000.0F);
		projectionBlock_.update(ProjectionBlock{camera().projection()});
	}

	void onResize(int /*width*/, int /*height*/) override {
		projectionBlock_.update(ProjectionBlock{camera().projection()});
	}

	void onUpdate(float deltaSeconds) override {
		// A hot reload relinks the program, and a fresh link has every uniform
		// block on point 0 and every sampler on unit 0. The watcher has
		// already polled by the time onUpdate runs, so this catches a reload
		// in the same frame it happened.
		if (shaders().reloadCount() != appliedReloads_) {
			configurePrograms();
		}

		orbit_.update(camera(), input());
		updateObjectRotation();
		lightTimer_.update(deltaSeconds);

		// input() reports nothing while ImGui owns the keyboard, so typing a
		// digit into a field does not also switch tables.
		if (input().keyPressed(GLFW_KEY_SPACE)) {
			mode_ = static_cast<ShadingMode>((static_cast<std::size_t>(mode_) + 1) % kModeCount);
		}
		if (input().keyPressed(GLFW_KEY_Y)) {
			useInfinity_ = !useInfinity_;
		}
		for (std::size_t i = 0; i < kTableCount; ++i) {
			if (input().keyPressed(GLFW_KEY_1 + static_cast<int>(i))) {
				currentTable_ = i;
			}
		}
		for (std::size_t i = 0; i < kMaterialCount; ++i) {
			if (input().keyPressed(kMaterials[i].key)) {
				material_ = static_cast<MaterialId>(i);
				uploadMaterial();
			}
		}
		if (input().keyPressed(GLFW_KEY_P)) {
			lightTimer_.togglePause();
		}
		if (input().keyPressed(GLFW_KEY_MINUS)) {
			lightTimer_.rewind(kScrubSeconds);
		}
		if (input().keyPressed(GLFW_KEY_EQUAL)) {
			lightTimer_.fastForward(kScrubSeconds);
		}
		if (input().keyPressed(GLFW_KEY_G)) {
			drawLights_ = !drawLights_;
		}
	}

	void onGui() override {
		if (ImGui::Begin("Tutorial 14")) {
			if (ImGui::CollapsingHeader("Shading", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawShadingControls();
			}
			if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawMaterialControls();
			}
			if (ImGui::CollapsingHeader("Object", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawObjectControls();
			}
			if (ImGui::CollapsingHeader("Textures")) {
				drawTexturePreviews();
			}
			if (ImGui::CollapsingHeader("Lights")) {
				drawLightControls();
			}
			if (ImGui::CollapsingHeader("Camera")) {
				drawCameraControls();
			}
		}
		ImGui::End();
	}

	void onRender() override {
		glc::MatrixStack& stack = matrices();
		stack.SetMatrix(camera().view());

		lightBlock_.update(buildLightBlock(stack.Top()));

		{
			const glc::MatrixStack::Frame objectFrame = stack.push();
			stack.RotateY(objectYaw_);
			stack.RotateX(objectPitch_);
			stack.Scale(useInfinity_ ? kInfinityScale : kPlaneScale);

			const glm::mat4& modelToCamera = stack.Top();
			const glm::mat3 normalModelToCamera(
			    glm::transpose(glm::inverse(glm::mat3(modelToCamera))));

			const ModeInfo& mode = modeInfo(mode_);
			const glc::Program& program = programs_[static_cast<std::size_t>(mode_)]->get();
			program.use();
			program.set("modelToCameraMatrix", modelToCamera);
			program.set("normalModelToCameraMatrix", normalModelToCamera);

			// Both textures are bound whichever mode is drawing, as in the
			// book, and the same sampler object goes on both units. A unit
			// that no sampler uniform in the current program points at is
			// simply not read. The reverse is also allowed: one texture on
			// two units with a different sampler on each.
			glc::bindTextureUnit(kGaussTextureUnit, gaussTextures_[currentTable_], sampler_);
			glc::bindTextureUnit(kShineTextureUnit, shininessTexture_, sampler_);

			// The VAO name is where the texture coordinates enter the
			// pipeline -- or do not. Same buffer, same positions and normals;
			// "lit-tex" additionally wires attribute 5 to the shader.
			const glc::Mesh& mesh = useInfinity_ ? infinityMesh_ : planeMesh_;
			mesh.render(mode.vao);
		}

		if (drawLights_) {
			drawLightMarkers(stack);
		}
	}

private:
	/// Closer than the book's 10: the table's rings are easier to see when
	/// the highlight covers more pixels. Scroll to taste.
	glc::OrbitController orbit_{glm::vec3{0.0F, 0.5F, 0.0F}, 7.0F, 45.0F, 25.0F};
	glc::CycleTimer lightTimer_{kOrbitSeconds};

	std::array<glc::ReloadableProgram*, kModeCount> programs_{};
	glc::ReloadableProgram* unlitProgram_{};

	glc::UniformBuffer projectionBlock_{glc::UniformBuffer::forType<ProjectionBlock>()};
	glc::UniformBuffer lightBlock_{glc::UniformBuffer::forType<LightBlock>()};
	glc::UniformBuffer materialBlock_{glc::UniformBuffer::forType<MaterialBlock>()};

	glc::Mesh infinityMesh_;
	glc::Mesh planeMesh_;
	glc::Mesh cubeMesh_;

	std::array<glc::Texture, kTableCount> gaussTextures_{};
	glc::Texture shininessTexture_;
	glc::Sampler sampler_;

	ShadingMode mode_{ShadingMode::FixedShininess};
	/// The book starts on the largest table, and the text about the rings on
	/// the plane is written about that one.
	std::size_t currentTable_{kTableCount - 1};
	MaterialId material_{MaterialId::Gold};
	float shininess_{kDefaultShininess};
	bool useInfinity_{true};
	bool drawLights_{true};
	float objectYaw_{0.0F};
	float objectPitch_{0.0F};
	int appliedReloads_{0};

	/// Everything that survives only until the next relink: the uniform block
	/// bindings, and the image unit each sampler reads from.
	void configurePrograms() {
		for (const glc::ReloadableProgram* lit : programs_) {
			const glc::Program& program = lit->get();
			program.bindUniformBlock("Material", kMaterialBlockBinding);
			program.bindUniformBlock("Light", kLightBlockBinding);
			program.bindUniformBlock("Projection", kProjectionBlockBinding);

			// A sampler uniform is set to a unit number, not a texture. Each
			// program declares one or both, hence setIfPresent.
			program.use();
			program.setIfPresent("gaussianTexture", static_cast<int>(kGaussTextureUnit));
			program.setIfPresent("shininessTexture", static_cast<int>(kShineTextureUnit));
		}
		unlitProgram_->get().bindUniformBlock("Projection", kProjectionBlockBinding);
		glc::Program::unuse();

		appliedReloads_ = shaders().reloadCount();
	}

	/// The book keeps both materials in one buffer and binds a range per
	/// draw; with two of them and one object, re-uploading 48 bytes on a key
	/// press is simpler and just as correct.
	void uploadMaterial() {
		const MaterialPreset& preset = kMaterials[static_cast<std::size_t>(material_)];
		materialBlock_.update(MaterialBlock{
		    .diffuseColor = preset.diffuseColor,
		    .specularColor = preset.specularColor,
		    .specularShininess = shininess_,
		    .padding = {},
		});
	}

	/// The book's ObjectPole, reduced to what this scene needs: right-drag
	/// turns the object so the highlight can be walked across it. The camera
	/// keeps the left button.
	void updateObjectRotation() {
		if (!input().mouseDown(GLFW_MOUSE_BUTTON_RIGHT)) {
			return;
		}
		const glm::vec2 delta = input().mouseDelta();
		objectYaw_ = wrapDegrees(objectYaw_ + (delta.x * kObjectDegreesPerPixel));
		objectPitch_ = wrapDegrees(objectPitch_ + (delta.y * kObjectDegreesPerPixel));
	}

	/// World-space position of the point light, on a circle at kLightHeight.
	[[nodiscard]] glm::vec3 lightPosition() const {
		const float angle = lightTimer_.alpha() * glm::two_pi<float>();
		return {std::cos(angle) * kLightOrbitRadius, kLightHeight,
		        std::sin(angle) * kLightOrbitRadius};
	}

	/// One directional light fixed in the world and one point light on a
	/// loop, both converted to camera space here so the shader never has to.
	[[nodiscard]] LightBlock buildLightBlock(const glm::mat4& worldToCamera) const {
		LightBlock block{};
		block.ambientIntensity = {0.2F, 0.2F, 0.2F, 1.0F};
		block.lightAttenuation = kLightAttenuation;

		block.lights[0].cameraSpaceLightPos = worldToCamera * glm::vec4{kSunDirection, 0.0F};
		block.lights[0].intensity = {0.6F, 0.6F, 0.6F, 1.0F};

		block.lights[1].cameraSpaceLightPos = worldToCamera * glm::vec4{lightPosition(), 1.0F};
		block.lights[1].intensity = {0.4F, 0.4F, 0.4F, 1.0F};

		return block;
	}

	/// A small cube riding the point light, and a big one far off in the
	/// direction the sun comes from.
	void drawLightMarkers(glc::MatrixStack& stack) {
		const glc::Program& program = unlitProgram_->get();
		program.use();
		program.set("objectColor", glm::vec4{1.0F});

		{
			const glc::MatrixStack::Frame lampFrame = stack.push();
			stack.Translate(lightPosition());
			stack.Scale(kLightMarkerScale);
			program.set("modelToCameraMatrix", stack.Top());
			cubeMesh_.render("flat");
		}
		{
			const glc::MatrixStack::Frame sunFrame = stack.push();
			stack.Translate(kSunDirection * kSunMarkerDistance);
			stack.Scale(kSunMarkerScale);
			program.set("modelToCameraMatrix", stack.Top());
			cubeMesh_.render("flat");
		}
	}

	void drawShadingControls() {
		for (std::size_t i = 0; i < kModeCount; ++i) {
			const auto candidate = static_cast<ShadingMode>(i);
			if (ImGui::RadioButton(kModes[i].label, mode_ == candidate)) {
				mode_ = candidate;
			}
		}
		ImGui::TextDisabled("Space cycles.");

		ImGui::SeparatorText("Gaussian table");
		ImGui::BeginDisabled(!modeInfo(mode_).usesTable);
		for (std::size_t i = 0; i < kTableCount; ++i) {
			ImGui::PushID(static_cast<int>(i));
			if (ImGui::RadioButton("##table", currentTable_ == i)) {
				currentTable_ = i;
			}
			ImGui::SameLine();
			ImGui::Text("%d x %d", cosAngleResolution(i), kShininessResolution);
			ImGui::SameLine();
			ImGui::TextDisabled("(%zu)", i + 1);
			ImGui::PopID();
		}
		ImGui::EndDisabled();
		ImGui::TextDisabled("cosine columns x shininess rows");
	}

	void drawMaterialControls() {
		for (std::size_t i = 0; i < kMaterialCount; ++i) {
			const auto candidate = static_cast<MaterialId>(i);
			if (ImGui::RadioButton(kMaterials[i].name, material_ == candidate)) {
				material_ = candidate;
				uploadMaterial();
			}
			ImGui::SameLine();
			// GLFW's printable key codes are their ASCII characters.
			ImGui::TextDisabled("(%c)", kMaterials[i].key);
		}

		const bool textured = modeInfo(mode_).usesShininessTexture;
		ImGui::BeginDisabled(textured);
		if (ImGui::SliderFloat("Shininess", &shininess_, kMinShininess, kMaxShininess, "%.3f")) {
			uploadMaterial();
		}
		ImGui::EndDisabled();
		if (textured) {
			ImGui::TextDisabled("Comes from the texture in this mode.");
		} else {
			ImGui::TextDisabled("Just a texture coordinate now: no table is rebuilt.");
		}
	}

	void drawObjectControls() {
		if (ImGui::RadioButton("Infinity symbol", useInfinity_)) {
			useInfinity_ = true;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Plane", !useInfinity_)) {
			useInfinity_ = false;
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(Y)");

		ImGui::SliderFloat("Yaw", &objectYaw_, -180.0F, 180.0F, "%.0f deg");
		ImGui::SliderFloat("Pitch", &objectPitch_, -180.0F, 180.0F, "%.0f deg");
		ImGui::TextDisabled("Or right-drag in the view.");
	}

	/// The textures themselves, drawn as pictures for once. The table is a
	/// plot of the function: cosine left to right, shininess bottom to top,
	/// and the bright wedge in the top-right corner is where a wide Gaussian
	/// meets a small angle.
	void drawTexturePreviews() {
		ImGui::Text("Gaussian table, S = cosine, T = shininess");
		drawTexturePreview(gaussTextures_[currentTable_],
		                   static_cast<float>(cosAngleResolution(currentTable_)) /
		                       static_cast<float>(kShininessResolution));

		ImGui::Text("Shininess texture, %d x %d", kShininessTextureWidth, kShininessTextureHeight);
		drawTexturePreview(shininessTexture_, static_cast<float>(kShininessTextureWidth) /
		                                          static_cast<float>(kShininessTextureHeight));
		ImGui::TextDisabled("Bright is wide and dull; dark is tight and shiny.");
	}

	/// Seeded from the timer every frame so the widgets track it and become
	/// scrub handles the moment they are touched.
	void drawLightControls() {
		bool paused = lightTimer_.isPaused();
		if (ImGui::Checkbox("Paused", &paused)) {
			lightTimer_.setPaused(paused);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(P)");
		ImGui::SameLine();
		if (ImGui::SmallButton("-0.5s")) {
			lightTimer_.rewind(kScrubSeconds);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("+0.5s")) {
			lightTimer_.fastForward(kScrubSeconds);
		}

		float alpha = lightTimer_.alpha();
		if (ImGui::SliderFloat("Orbit", &alpha, 0.0F, 1.0F, "%.2f")) {
			lightTimer_.setAlpha(alpha);
		}

		ImGui::Checkbox("Show light markers", &drawLights_);
		ImGui::SameLine();
		ImGui::TextDisabled("(G)");
	}

	void drawCameraControls() {
		const glm::vec3& target = orbit_.target();
		ImGui::Text("target  %6.1f %6.1f %6.1f", static_cast<double>(target.x),
		            static_cast<double>(target.y), static_cast<double>(target.z));
		ImGui::Text("distance %.1f   yaw %.1f   pitch %.1f", static_cast<double>(orbit_.distance()),
		            static_cast<double>(orbit_.yaw()), static_cast<double>(orbit_.pitch()));
	}
};

} // namespace

int main() {
	return glc::runApp<MaterialTexture>();
}
