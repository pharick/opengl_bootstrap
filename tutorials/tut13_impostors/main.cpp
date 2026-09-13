// Tutorial 13 (Lies and Impostors), first part: the basic sphere impostor.
//
// Every chapter so far has lit a mesh. This one lights a surface that has no
// mesh at all: each sphere can be drawn either as gltut's UnitSphere or as a
// single camera-facing square whose fragment shader computes, per pixel, where
// a perfect sphere would have been. The lighting model is identical in both
// cases -- it only ever sees a position and a normal, and does not know which
// of the two produced them.
//
// Toggle a sphere between the two with the 1-4 keys or the panel, and look at
// its silhouette: the mesh is visibly a polyhedron (this chapter's sphere is a
// coarse one on purpose), the impostor is a perfect disc from any distance.
// Then move the camera off to the side and watch the impostor pass through the
// ground plane -- its depth is the square's, not the sphere's. That is the flaw
// the rest of the chapter is about.

#include <glcore/app.hpp>
#include <glcore/camera.hpp>
#include <glcore/cycle_timer.hpp>
#include <glcore/gl_check.hpp>
#include <glcore/mesh.hpp>
#include <glcore/paths.hpp>
#include <glcore/scoped_bind.hpp>
#include <glcore/shader_watcher.hpp>
#include <glcore/uniform_buffer.hpp>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <imgui.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace {

constexpr GLuint kProjectionBlockBinding = 0;
constexpr GLuint kLightBlockBinding = 1;
constexpr GLuint kMaterialBlockBinding = 2;

/// One orbit of the point light and of every orbiting sphere.
constexpr float kOrbitSeconds = 6.0F;

/// Seconds moved per press of a scrub button; the book's `-` and `=` keys.
constexpr float kScrubSeconds = 0.5F;

constexpr float kLightHeight = 20.0F;
constexpr float kLightOrbitRadius = 20.0F;

/// The lamp falls to half brightness 25 units out, under inverse-square falloff.
constexpr float kHalfLightDistance = 25.0F;
constexpr float kLightAttenuation = 1.0F / (kHalfLightDistance * kHalfLightDistance);

/// The book's LargePlane.xml is UnitPlane at 60x.
constexpr float kGroundScale = 60.0F;

/// gltut's spheres have a radius of 0.5, so a sphere of radius r is scaled 2r.
constexpr float kUnitSphereDiameter = 2.0F;

constexpr float kLightMarkerScale = 0.5F;

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

/// Index into the material buffer, so the order here is the order in kMaterials.
enum class MaterialId : std::uint8_t {
	Terrain = 0,
	BlueShiny,
	GoldMetal,
	DullGrey,
	BlackShiny,
};

constexpr std::size_t kMaterialCount = 5;

constexpr std::array<MaterialBlock, kMaterialCount> kMaterials{
    {
        {
            .diffuseColor = {0.5F, 0.5F, 0.5F, 1.0F},
            .specularColor = {0.5F, 0.5F, 0.5F, 1.0F},
            .specularShininess = 0.6F,
            .padding = {},
        },
        {
            .diffuseColor = {0.1F, 0.1F, 0.8F, 1.0F},
            .specularColor = {0.8F, 0.8F, 0.8F, 1.0F},
            .specularShininess = 0.1F,
            .padding = {},
        },
        {
            // Gold: the specular is the diffuse at three quarters. Metals tint
            // their highlights, where plastics and paints bounce white.
            .diffuseColor = {0.803F, 0.709F, 0.15F, 1.0F},
            .specularColor = {0.803F * 0.75F, 0.709F * 0.75F, 0.15F * 0.75F, 1.0F},
            .specularShininess = 0.18F,
            .padding = {},
        },
        {
            .diffuseColor = {0.4F, 0.4F, 0.4F, 1.0F},
            .specularColor = {0.1F, 0.1F, 0.1F, 1.0F},
            .specularShininess = 0.8F,
            .padding = {},
        },
        {
            .diffuseColor = {0.05F, 0.05F, 0.05F, 1.0F},
            .specularColor = {0.95F, 0.95F, 0.95F, 1.0F},
            .specularShininess = 0.3F,
            .padding = {},
        },
    },
};

/// Rounds `size` up to the next multiple of `alignment`.
[[nodiscard]] constexpr std::size_t alignUp(std::size_t size, std::size_t alignment) {
	return ((size + alignment - 1) / alignment) * alignment;
}

/// Bytes between consecutive materials: sizeof(MaterialBlock) rounded up to
/// what glBindBufferRange will accept as an offset.
[[nodiscard]] std::size_t materialStride() {
	return alignUp(sizeof(MaterialBlock),
	               static_cast<std::size_t>(glc::UniformBuffer::offsetAlignment()));
}

/// Every material in one buffer, each on an offset-alignment boundary so one
/// slice can be bound per draw with glBindBufferRange. Same scheme as tut12.
class MaterialSet {
public:
	MaterialSet()
	    : stride_{materialStride()},
	      buffer_{glc::UniformBuffer::create(stride_ * kMaterialCount, GL_STATIC_DRAW)} {
		for (std::size_t i = 0; i < kMaterials.size(); ++i) {
			buffer_.update(kMaterials[i], i * stride_);
		}
	}

	/// Binds one material to `bindingPoint` for the next draw.
	void bind(GLuint bindingPoint, MaterialId id) const {
		buffer_.bindRange(bindingPoint, static_cast<std::size_t>(id) * stride_,
		                  sizeof(MaterialBlock));
	}

private:
	// Declaration order is load-bearing: buffer_'s initializer reads stride_.
	std::size_t stride_;
	glc::UniformBuffer buffer_;
};

/// The four spheres, in the order the 1-4 keys and the panel refer to them.
enum class SphereId : std::uint8_t { Blue = 0, Grey, Black, Gold };

constexpr std::size_t kSphereCount = 4;

constexpr std::array<const char*, kSphereCount> kSphereLabels{
    "Blue, centre",
    "Grey, tilted orbit",
    "Black marble, left",
    "Gold, right",
};

class Impostors final : public glc::App {
public:
	Impostors()
	    : glc::App({
	          .window = {.title = "gltut 13 -- Lies and Impostors"},
	          .depthTest = true,
	          .cullFace = true,
	          .frontFace = GL_CW, // gltut's meshes are wound clockwise
	          .clearColor = {0.75F, 0.75F, 1.0F, 1.0F},
	      }) {}

protected:
	void onInit() override {
		groundMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitPlane.xml"));
		sphereMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitSphereCoarse.xml"));
		cubeMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitCubeLit.xml"));

		meshProgram_ = &shaders().add(glc::paths::tutorialShader("mesh.vert"),
		                              glc::paths::tutorialShader("mesh.frag"));
		impostorProgram_ = &shaders().add(glc::paths::tutorialShader("impostor.vert"),
		                                  glc::paths::tutorialShader("impostor.frag"));
		unlitProgram_ = &shaders().add(glc::paths::tutorialShader("unlit.vert"),
		                               glc::paths::tutorialShader("unlit.frag"));

		for (const glc::ReloadableProgram* program : {meshProgram_, impostorProgram_}) {
			program->get().bindUniformBlock("Projection", kProjectionBlockBinding);
			program->get().bindUniformBlock("Light", kLightBlockBinding);
			program->get().bindUniformBlock("Material", kMaterialBlockBinding);
		}
		unlitProgram_->get().bindUniformBlock("Projection", kProjectionBlockBinding);

		projectionBlock_.bindToPoint(kProjectionBlockBinding);
		lightBlock_.bindToPoint(kLightBlockBinding);
		// The material buffer holds all five; a slice is bound per draw.

		camera().setPerspective(45.0F, aspect(), 1.0F, 1000.0F);
		projectionBlock_.update(ProjectionBlock{camera().projection()});

		// The whole point: a VAO with nothing in it. The impostor's vertex
		// shader gets four invocations from glDrawArrays and reads gl_VertexID;
		// there is no buffer for it to read anything else from.
		impostorVao_ = glc::VertexArray::create();
	}

	void onResize(int /*width*/, int /*height*/) override {
		projectionBlock_.update(ProjectionBlock{camera().projection()});
	}

	void onUpdate(float deltaSeconds) override {
		orbit_.update(camera(), input());
		orbitTimer_.update(deltaSeconds);

		// input() reports nothing while ImGui owns the keyboard, so typing a
		// digit into a field does not also flip a sphere.
		for (std::size_t i = 0; i < kSphereCount; ++i) {
			if (input().keyPressed(GLFW_KEY_1 + static_cast<int>(i))) {
				drawImpostor_[i] = !drawImpostor_[i];
			}
		}
		if (input().keyPressed(GLFW_KEY_P)) {
			orbitTimer_.togglePause();
		}
		if (input().keyPressed(GLFW_KEY_MINUS)) {
			orbitTimer_.rewind(kScrubSeconds);
		}
		if (input().keyPressed(GLFW_KEY_EQUAL)) {
			orbitTimer_.fastForward(kScrubSeconds);
		}
		if (input().keyPressed(GLFW_KEY_G)) {
			drawLight_ = !drawLight_;
		}
	}

	void onGui() override {
		if (ImGui::Begin("Tutorial 13")) {
			if (ImGui::CollapsingHeader("Spheres", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawSphereControls();
			}
			if (ImGui::CollapsingHeader("Time", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawTimeControls();
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
			const glc::MatrixStack::Frame groundFrame = stack.push();
			stack.Scale(kGroundScale);
			drawMesh(groundMesh_, MaterialId::Terrain);
		}

		const float alpha = orbitTimer_.alpha();

		drawSphere(stack, {0.0F, 10.0F, 0.0F}, 4.0F, MaterialId::BlueShiny, SphereId::Blue);
		drawSphereOrbit(stack, {0.0F, 10.0F, 0.0F}, {0.6F, 0.8F, 0.0F}, 20.0F, alpha, 2.0F,
		                MaterialId::DullGrey, SphereId::Grey);
		drawSphereOrbit(stack, {-10.0F, 1.0F, 0.0F}, {0.0F, 1.0F, 0.0F}, 10.0F, alpha, 1.0F,
		                MaterialId::BlackShiny, SphereId::Black);
		drawSphereOrbit(stack, {10.0F, 1.0F, 0.0F}, {0.0F, 1.0F, 0.0F}, 10.0F, alpha * 2.0F, 1.0F,
		                MaterialId::GoldMetal, SphereId::Gold);

		if (drawLight_) {
			const glc::MatrixStack::Frame lightFrame = stack.push();
			stack.Translate(lightPosition());
			stack.Scale(kLightMarkerScale);

			const glc::Program& program = unlitProgram_->get();
			program.use();
			program.set("modelToCameraMatrix", stack.Top());
			program.set("objectColor", glm::vec4{1.0F});
			cubeMesh_.render("flat");
		}
	}

private:
	glc::OrbitController orbit_{glm::vec3{0.0F, 8.0F, 0.0F}, 60.0F, 30.0F, 20.0F};
	glc::CycleTimer orbitTimer_{kOrbitSeconds};

	glc::ReloadableProgram* meshProgram_{};
	glc::ReloadableProgram* impostorProgram_{};
	glc::ReloadableProgram* unlitProgram_{};

	glc::UniformBuffer projectionBlock_{glc::UniformBuffer::forType<ProjectionBlock>()};
	glc::UniformBuffer lightBlock_{glc::UniformBuffer::forType<LightBlock>()};
	MaterialSet materials_;

	glc::Mesh groundMesh_;
	glc::Mesh sphereMesh_;
	glc::Mesh cubeMesh_;
	glc::VertexArray impostorVao_;

	std::array<bool, kSphereCount> drawImpostor_{};
	bool drawLight_{true};

	/// World-space position of the point light, on a circle at kLightHeight.
	[[nodiscard]] glm::vec3 lightPosition() const {
		const float angle = orbitTimer_.alpha() * glm::two_pi<float>();
		return {std::cos(angle) * kLightOrbitRadius, kLightHeight,
		        std::sin(angle) * kLightOrbitRadius};
	}

	/// One directional light fixed in the world and one point light on a
	/// loop, both converted to camera space here so the shader never has to.
	[[nodiscard]] LightBlock buildLightBlock(const glm::mat4& worldToCamera) const {
		LightBlock block{};
		block.ambientIntensity = {0.2F, 0.2F, 0.2F, 1.0F};
		block.lightAttenuation = kLightAttenuation;

		block.lights[0].cameraSpaceLightPos = worldToCamera * glm::vec4{0.707F, 0.707F, 0.0F, 0.0F};
		block.lights[0].intensity = {0.6F, 0.6F, 0.6F, 1.0F};

		block.lights[1].cameraSpaceLightPos = worldToCamera * glm::vec4{lightPosition(), 1.0F};
		block.lights[1].intensity = {0.4F, 0.4F, 0.4F, 1.0F};

		return block;
	}

	/// Draws `mesh` with the top of the stack as its model matrix, through one
	/// of its named VAOs or, with no name, its main one -- UnitPlane.xml has no
	/// named VAOs at all. Uniform scales only in this scene, but the inverse
	/// transpose is cheap and does not need revisiting when that changes.
	void drawMesh(const glc::Mesh& mesh, MaterialId materialId,
	              std::optional<std::string_view> vaoName = std::nullopt) {
		materials_.bind(kMaterialBlockBinding, materialId);

		const glm::mat4& modelToCamera = matrices().Top();
		const glm::mat3 normalModelToCamera(glm::transpose(glm::inverse(glm::mat3(modelToCamera))));

		const glc::Program& program = meshProgram_->get();
		program.use();
		program.set("modelToCameraMatrix", modelToCamera);
		program.set("normalModelToCameraMatrix", normalModelToCamera);

		if (vaoName.has_value()) {
			mesh.render(*vaoName);
		} else {
			mesh.render();
		}
	}

	/// A sphere of `radius` at `position` (relative to the top of the stack),
	/// as a mesh or as an impostor depending on the sphere's toggle.
	///
	/// The impostor path needs no model matrix at all: the vertex shader builds
	/// the square in camera space, so all it wants is the centre in camera
	/// space and the radius. That is the entire per-object cost.
	void drawSphere(glc::MatrixStack& stack, const glm::vec3& position, float radius,
	                MaterialId materialId, SphereId sphereId) {
		if (!drawImpostor_[static_cast<std::size_t>(sphereId)]) {
			const glc::MatrixStack::Frame sphereFrame = stack.push();
			stack.Translate(position);
			stack.Scale(radius * kUnitSphereDiameter);
			drawMesh(sphereMesh_, materialId, "lit");
			return;
		}

		materials_.bind(kMaterialBlockBinding, materialId);

		const glm::vec3 cameraSpherePos{stack.Top() * glm::vec4{position, 1.0F}};

		const glc::Program& program = impostorProgram_->get();
		program.use();
		program.set("cameraSpherePos", cameraSpherePos);
		program.set("sphereRadius", radius);

		const glc::ScopedBind bind{impostorVao_};
		GLC_CHECK(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
	}

	/// A sphere circling `orbitCenter` about `orbitAxis`, `orbitAlpha` of the
	/// way round. The starting point is wherever the axis crossed with world up
	/// lands -- or with world right, for an axis that *is* world up.
	void drawSphereOrbit(glc::MatrixStack& stack, const glm::vec3& orbitCenter,
	                     const glm::vec3& orbitAxis, float orbitRadius, float orbitAlpha,
	                     float sphereRadius, MaterialId materialId, SphereId sphereId) {
		const glc::MatrixStack::Frame orbitFrame = stack.push();

		stack.Translate(orbitCenter);
		stack.Rotate(orbitAxis, 360.0F * orbitAlpha);

		glm::vec3 offsetDir = glm::cross(orbitAxis, glm::vec3{0.0F, 1.0F, 0.0F});
		if (glm::length(offsetDir) < 0.001F) {
			offsetDir = glm::cross(orbitAxis, glm::vec3{1.0F, 0.0F, 0.0F});
		}
		stack.Translate(glm::normalize(offsetDir) * orbitRadius);

		drawSphere(stack, glm::vec3{0.0F}, sphereRadius, materialId, sphereId);
	}

	void drawSphereControls() {
		ImGui::TextUnformatted("Impostor");
		for (std::size_t i = 0; i < kSphereCount; ++i) {
			ImGui::PushID(static_cast<int>(i));
			ImGui::Checkbox(kSphereLabels[i], &drawImpostor_[i]);
			ImGui::SameLine();
			ImGui::TextDisabled("(%zu)", i + 1);
			ImGui::PopID();
		}
		if (ImGui::SmallButton("All impostors")) {
			drawImpostor_.fill(true);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("All meshes")) {
			drawImpostor_.fill(false);
		}

		ImGui::Checkbox("Show light marker", &drawLight_);
		ImGui::SameLine();
		ImGui::TextDisabled("(G)");
	}

	/// Seeded from the timer every frame so the widgets track it and become
	/// scrub handles the moment they are touched.
	void drawTimeControls() {
		bool paused = orbitTimer_.isPaused();
		if (ImGui::Checkbox("Paused", &paused)) {
			orbitTimer_.setPaused(paused);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(P)");
		ImGui::SameLine();
		if (ImGui::SmallButton("-0.5s")) {
			orbitTimer_.rewind(kScrubSeconds);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("+0.5s")) {
			orbitTimer_.fastForward(kScrubSeconds);
		}

		float alpha = orbitTimer_.alpha();
		if (ImGui::SliderFloat("Orbit", &alpha, 0.0F, 1.0F, "%.2f")) {
			orbitTimer_.setAlpha(alpha);
		}
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
	return glc::runApp<Impostors>();
}
