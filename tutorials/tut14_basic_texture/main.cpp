// Tutorial 14 (Textures are not Pictures): the Basic Texture example.
//
// The first texture in the book is deliberately not a picture. It is a look-up
// table: the Gaussian specular term from Tutorial 11, tabulated over its one
// remaining argument and stored as a row of bytes, so the fragment shader can
// replace an acos, a divide, a square and an exp with a single fetch.
//
// The scene is gltut's golden infinity symbol under a fixed directional light
// and an orbiting point light. Spacebar switches the object between the shader
// that computes the Gaussian (shader_gaussian.frag) and the one that looks it
// up (texture_gaussian.frag); the 1-4 keys pick a table of 64, 128, 256 or 512
// entries. At 64 the highlight is visibly stepped -- you can count the texels.
// At 512 it is indistinguishable from the computed one.
//
// Along the way this is the chapter that introduces the texture object, the
// sampler type in GLSL, texture image units and sampler objects, and the
// pixel-transfer split between "the format GL stores" and "the bytes you hand
// it". Each of those has a comment where it happens.

#include <glcore/app.hpp>
#include <glcore/cycle_timer.hpp>
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
#include <vector>

namespace {

constexpr GLuint kMaterialBlockBinding = 0;
constexpr GLuint kLightBlockBinding = 1;
constexpr GLuint kProjectionBlockBinding = 2;

/// The texture image unit the look-up table is bound to. A sampler uniform is
/// set to a unit, and a texture is bound to a unit: the unit is the only thing
/// the two sides of the association have in common.
constexpr GLuint kGaussTextureUnit = 0;

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

constexpr float kObjectScale = 2.0F;

/// Right-drag spins the object; this is the book's ObjectPole rate.
constexpr float kObjectDegreesPerPixel = 90.0F / 250.0F;

/// Gold, with the specular tinted like the diffuse -- a metal, not a plastic.
constexpr glm::vec4 kDiffuseColor{1.0F, 0.673F, 0.043F, 1.0F};
constexpr float kSpecularFraction = 0.4F;

/// Gaussian width in radians. The book fixes this at compile time; here it is
/// a slider, because watching every table get rebuilt when it moves is the
/// clearest way to see what the table has baked into it.
constexpr float kDefaultShininess = 0.2F;
constexpr float kMinShininess = 0.02F;
constexpr float kMaxShininess = 1.0F;

/// The 1-4 keys, smallest table first. Each is twice the last, so the banding
/// visibly halves from one key to the next.
constexpr std::size_t kTableCount = 4;
constexpr std::size_t kSmallestTable = 64;

[[nodiscard]] constexpr GLsizei tableSize(std::size_t level) {
	return static_cast<GLsizei>(kSmallestTable << level);
}

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

/// The Gaussian specular term, tabulated over the whole range of its argument.
///
/// Entry i holds F(cos) for cos = i / (resolution - 1), so the two ends of the
/// table land exactly on 0 and 1 rather than half a texel inside them. The
/// shininess goes in here and nowhere else the texture path can see.
///
/// Each entry is a *normalized integer*: a float on [0, 1] stored as a byte,
/// with 255 standing for 1.0. That is what the GL_R8 format means, it is a
/// quarter the size of a float texture, and the shader is none the wiser --
/// the sampler divides by 255 on the way out and hands back a float.
[[nodiscard]] std::vector<GLubyte> buildGaussianTable(GLsizei resolution, float shininess) {
	std::vector<GLubyte> table(static_cast<std::size_t>(resolution));

	for (std::size_t i = 0; i < table.size(); ++i) {
		const float cosAngle = static_cast<float>(i) / static_cast<float>(resolution - 1);
		const float exponent = std::acos(cosAngle) / shininess;
		const float gaussian = std::exp(-(exponent * exponent));
		table[i] = static_cast<GLubyte>(gaussian * 255.0F);
	}
	return table;
}

/// One table as a 1D texture.
///
/// The three trailing arguments of a glTexImage call describe the bytes being
/// handed over -- one component (GL_RED) per texel, each an unsigned byte --
/// and the internal format describes what GL should keep: GL_R8, one 8-bit
/// normalized channel. They match here, so no conversion happens on upload,
/// but they need not: GL_R16 would have been legal and would have cost a
/// conversion. The layout GL actually uses for the texels is its own business.
[[nodiscard]] glc::Texture makeGaussianTexture(GLsizei resolution, float shininess) {
	const std::vector<GLubyte> table = buildGaussianTable(resolution, shininess);
	return glc::makeTexture1D(resolution, GL_R8, GL_RED, GL_UNSIGNED_BYTE, table.data());
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

class BasicTexture final : public glc::App {
public:
	BasicTexture()
	    : glc::App({
	          .window = {.title = "gltut 14 -- Textures are not Pictures"},
	          .depthTest = true,
	          .cullFace = true,
	          .frontFace = GL_CW, // gltut's meshes are wound clockwise
	          .clearColor = {0.75F, 0.75F, 1.0F, 1.0F},
	      }) {}

protected:
	void onInit() override {
		objectMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/Infinity.xml"));
		cubeMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitCubeLit.xml"));

		shaderProgram_ = &shaders().add(glc::paths::tutorialShader("mesh.vert"),
		                                glc::paths::tutorialShader("shader_gaussian.frag"));
		textureProgram_ = &shaders().add(glc::paths::tutorialShader("mesh.vert"),
		                                 glc::paths::tutorialShader("texture_gaussian.frag"));
		unlitProgram_ = &shaders().add(glc::paths::tutorialShader("unlit.vert"),
		                               glc::paths::tutorialShader("unlit.frag"));
		configurePrograms();

		projectionBlock_.bindToPoint(kProjectionBlockBinding);
		lightBlock_.bindToPoint(kLightBlockBinding);
		materialBlock_.bindToPoint(kMaterialBlockBinding);
		uploadMaterial();

		rebuildTables();

		// How the table is read lives in a sampler object, not in the texture,
		// so one of these serves all four.
		//
		// GL_NEAREST is deliberate: this chapter wants to *see* the texels,
		// and blending between them would smooth over exactly the artifact
		// the 1-4 keys exist to show. GL_CLAMP_TO_EDGE is not cosmetic -- a
		// coordinate that falls off the end of the table must land on that
		// end, not wrap round to the other one.
		gaussSampler_ = glc::makeSampler({
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
			useTexture_ = !useTexture_;
		}
		for (std::size_t i = 0; i < kTableCount; ++i) {
			if (input().keyPressed(GLFW_KEY_1 + static_cast<int>(i))) {
				currentTable_ = i;
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
			if (ImGui::CollapsingHeader("Specular", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawSpecularControls();
			}
			if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawLightControls();
			}
			if (ImGui::CollapsingHeader("Object")) {
				drawObjectControls();
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
			stack.Scale(kObjectScale);

			const glm::mat4& modelToCamera = stack.Top();
			const glm::mat3 normalModelToCamera(
			    glm::transpose(glm::inverse(glm::mat3(modelToCamera))));

			const glc::Program& program =
			    useTexture_ ? textureProgram_->get() : shaderProgram_->get();
			program.use();
			program.set("modelToCameraMatrix", modelToCamera);
			program.set("normalModelToCameraMatrix", normalModelToCamera);

			// Bound whichever program is drawing, as in the book. That is
			// harmless: what sits in an image unit means nothing to a program
			// with no sampler uniform pointing at it.
			//
			// Underneath, this is glActiveTexture(GL_TEXTURE0 + unit) followed
			// by glBindTexture -- there is no bind-to-unit call the way there
			// is glBindBufferRange for UBOs -- and then glBindSampler(unit, ...),
			// which for some reason does take the unit directly.
			glc::bindTextureUnit(kGaussTextureUnit, gaussTextures_[currentTable_], gaussSampler_,
			                     GL_TEXTURE_1D);

			objectMesh_.render("lit");
		}

		if (drawLights_) {
			drawLightMarkers(stack);
		}
	}

private:
	/// Closer than the book's 10: the banding is the exhibit, and it is easier
	/// to see when the highlight covers more pixels. Scroll to taste.
	glc::OrbitController orbit_{glm::vec3{0.0F, 0.5F, 0.0F}, 7.0F, 45.0F, 25.0F};
	glc::CycleTimer lightTimer_{kOrbitSeconds};

	glc::ReloadableProgram* shaderProgram_{};
	glc::ReloadableProgram* textureProgram_{};
	glc::ReloadableProgram* unlitProgram_{};

	glc::UniformBuffer projectionBlock_{glc::UniformBuffer::forType<ProjectionBlock>()};
	glc::UniformBuffer lightBlock_{glc::UniformBuffer::forType<LightBlock>()};
	glc::UniformBuffer materialBlock_{glc::UniformBuffer::forType<MaterialBlock>(GL_STATIC_DRAW)};

	glc::Mesh objectMesh_;
	glc::Mesh cubeMesh_;

	std::array<glc::Texture, kTableCount> gaussTextures_{};
	glc::Sampler gaussSampler_;

	bool useTexture_{false};
	std::size_t currentTable_{0};
	float shininess_{kDefaultShininess};
	bool drawLights_{true};
	float objectYaw_{0.0F};
	float objectPitch_{0.0F};
	int appliedReloads_{0};

	/// Everything that survives only until the next relink: the uniform block
	/// bindings, and the image unit the sampler reads from.
	void configurePrograms() {
		for (const glc::ReloadableProgram* lit : {shaderProgram_, textureProgram_}) {
			const glc::Program& program = lit->get();
			program.bindUniformBlock("Material", kMaterialBlockBinding);
			program.bindUniformBlock("Light", kLightBlockBinding);
			program.bindUniformBlock("Projection", kProjectionBlockBinding);

			// From this side of the API a sampler is an ordinary integer
			// uniform, and its value is the image unit -- not the texture.
			// Only texture_gaussian.frag declares one, hence setIfPresent.
			program.use();
			program.setIfPresent("gaussianTexture", static_cast<int>(kGaussTextureUnit));
		}
		unlitProgram_->get().bindUniformBlock("Projection", kProjectionBlockBinding);
		glc::Program::unuse();

		appliedReloads_ = shaders().reloadCount();
	}

	void uploadMaterial() {
		materialBlock_.update(MaterialBlock{
		    .diffuseColor = kDiffuseColor,
		    .specularColor = kDiffuseColor * kSpecularFraction,
		    .specularShininess = shininess_,
		    .padding = {},
		});
	}

	/// All four tables, for the current shininess.
	///
	/// This is the real cost of the optimization. The shader path reads the
	/// shininess from the material block and would follow a change for free;
	/// the texture path has it baked in, and the only way to hand it a new
	/// value is a new texture.
	void rebuildTables() {
		for (std::size_t i = 0; i < kTableCount; ++i) {
			gaussTextures_[i] = makeGaussianTexture(tableSize(i), shininess_);
		}
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

	void drawSpecularControls() {
		if (ImGui::RadioButton("Computed in the shader", !useTexture_)) {
			useTexture_ = false;
		}
		if (ImGui::RadioButton("Looked up in a texture", useTexture_)) {
			useTexture_ = true;
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(Space)");

		ImGui::SeparatorText("Look-up table");
		ImGui::BeginDisabled(!useTexture_);
		for (std::size_t i = 0; i < kTableCount; ++i) {
			ImGui::PushID(static_cast<int>(i));
			if (ImGui::RadioButton("##table", currentTable_ == i)) {
				currentTable_ = i;
			}
			ImGui::SameLine();
			ImGui::Text("%d texels", tableSize(i));
			ImGui::SameLine();
			ImGui::TextDisabled("(%zu)", i + 1);
			ImGui::PopID();
		}
		ImGui::EndDisabled();

		ImGui::SeparatorText("Material");
		if (ImGui::SliderFloat("Shininess", &shininess_, kMinShininess, kMaxShininess, "%.3f")) {
			uploadMaterial();
			rebuildTables();
		}
		ImGui::TextDisabled("Baked into every table: moving this rebuilds all four.");
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

	void drawObjectControls() {
		ImGui::SliderFloat("Yaw", &objectYaw_, -180.0F, 180.0F, "%.0f deg");
		ImGui::SliderFloat("Pitch", &objectPitch_, -180.0F, 180.0F, "%.0f deg");
		ImGui::TextDisabled("Or right-drag in the view.");
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
	return glc::runApp<BasicTexture>();
}
