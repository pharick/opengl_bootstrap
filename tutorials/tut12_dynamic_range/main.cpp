#include "glcore/app.hpp"
#include "glcore/camera.hpp"
#include "glcore/cycle_timer.hpp"
#include "glcore/mesh.hpp"
#include "glcore/paths.hpp"
#include "glcore/shader_watcher.hpp"
#include "glcore/uniform_buffer.hpp"

#include "lights.hpp"
#include "materials.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>

#include <imgui.h>

#include <cmath>
#include <cstdint>
#include <format>
#include <numbers>
#include <optional>
#include <string_view>

namespace {

/// The sun is overhead at alpha 0, so the book's raw hours put noon at 0 and
/// midnight at 12. Shifting by half a day turns that into a clock that reads
/// the way anyone would expect: dark late evening through early morning.
constexpr float kClockOffsetHours = 12.0F;

/// Seconds moved per press of a scrub button.
constexpr float kScrubSeconds = 1.0F;

/// The sun is directional, so it has no position -- the marker is parked far
/// enough along its direction to read as "over there", inside the far plane.
constexpr float kSunMarkerDistance = 500.0F;
constexpr float kSunMarkerScale = 30.0F;

/// Seconds per full turn of the tetrahedron. A rotating object sweeps a
/// specular highlight across every surface orientation without the camera
/// having to move, which is the cheapest way to watch a highlight clip.
constexpr float kTetraSpinSeconds = 2.5F;

/// gltut leaves the point-light markers as unscaled unit cubes, which in a
/// 220-unit scene are single pixels. Big enough to aim at instead.
constexpr float kPointMarkerScale = 3.0F;

struct ProjectionBlock {
	glm::mat4 cameraToClipMatrix;
};
static_assert(sizeof(ProjectionBlock) == 64);

constexpr GLuint kProjectionBlockBinding = 0;
constexpr GLuint kLightBlockBinding = 1;
constexpr GLuint kMaterialBlockBinding = 2;

/// Where an object's diffuse colour comes from, which also settles which
/// program can draw it. One parameter rather than two, because a program and a
/// VAO that disagree fail silently rather than erroring.
enum class Shading : std::uint8_t {
	VertexColor, ///< mesh supplies attribute 1; material tints it
	Material,    ///< no vertex colours; the material is the colour
};

class DynamicRange final : public glc::App {
public:
	DynamicRange()
	    : glc::App({
	          .window = {.title = "gltut 12 -- Dynamic Range"},
	          .depthTest = true,
	          .cullFace = true,
	          .frontFace = GL_CW, // gltut's meshes are wound clockwise
	      }) {}

protected:
	void onInit() override {
		// meshes
		groundMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/Ground.xml"));
		tetrahedronMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitTetrahedron.xml"));
		cubeMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitCubeLit.xml"));
		cylinderMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitCylinder.xml"));
		sphereMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitSphere.xml"));

		// program for light markers
		unlitProgram_ = &shaders().add(glc::paths::tutorialShader("unlit.vert"),
		                               glc::paths::tutorialShader("unlit.frag"));
		unlitProgram_->get().bindUniformBlock("Projection", kProjectionBlockBinding);

		// vertex color program
		litVertexColorProgram_ = &shaders().add(glc::paths::tutorialShader("lit_vertex_color.vert"),
		                                        glc::paths::tutorialShader("lit.frag"));

		// material program
		litMaterialProgram_ = &shaders().add(glc::paths::tutorialShader("lit_material.vert"),
		                                     glc::paths::tutorialShader("lit.frag"));

		// bind and update projection block
		projectionBlock_.bindToPoint(kProjectionBlockBinding);
		litVertexColorProgram_->get().bindUniformBlock("Projection", kProjectionBlockBinding);
		litMaterialProgram_->get().bindUniformBlock("Projection", kProjectionBlockBinding);

		camera().setPerspective(45.0F, aspect(), 1.0F, 1000.0F);
		projectionBlock_.update(ProjectionBlock{camera().projection()});

		// bind light block
		lightBlock_.bindToPoint(kLightBlockBinding);
		litVertexColorProgram_->get().bindUniformBlock("Light", kLightBlockBinding);
		litMaterialProgram_->get().bindUniformBlock("Light", kLightBlockBinding);

		// The material buffer holds all six; a slice is bound per draw, so
		// there is no bindToPoint here -- only the per-program block binding.
		litVertexColorProgram_->get().bindUniformBlock("Material", kMaterialBlockBinding);
		litMaterialProgram_->get().bindUniformBlock("Material", kMaterialBlockBinding);

		// set up fly controller
		fly_.settings().unitsPerSecond = 50.0F;
	}

	void onResize(int /*width*/, int /*height*/) override {
		projectionBlock_.update(ProjectionBlock{camera().projection()});
	}

	void onUpdate(float deltaSeconds) override {
		fly_.update(camera(), input(), deltaSeconds);
		lightsManager_.update(deltaSeconds);
		tetraTimer_.update(deltaSeconds);
	}

	void onGui() override {
		if (ImGui::Begin("Tutorial 12")) {
			if (ImGui::CollapsingHeader("Time", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawTimeControls();
			}
			if (ImGui::CollapsingHeader("Lighting")) {
				drawLightingControls();
			}
			if (ImGui::CollapsingHeader("Camera")) {
				drawCameraControls();
			}
		}
		ImGui::End();
	}

	void onRender() override {
		// The sky is interpolated too, so it has to be pushed every frame.
		setClearColor(lightsManager_.backgroundColor());

		lightBlock_.update(lightsManager_.toBlock(camera().view()));

		glc::MatrixStack& stack = matrices();
		stack.SetMatrix(camera().view());

		{
			const glc::MatrixStack::Frame groundFrame = stack.push();

			stack.RotateX(-90.0F);

			drawObject(groundMesh_, Shading::VertexColor, MaterialId::Ground);
		}

		{
			const glc::MatrixStack::Frame tetrahedronFrame = stack.push();

			stack.Translate(75.0F, 5.0F, 75.0F);
			stack.RotateY(360.0F * tetraTimer_.alpha());
			stack.Scale(10.0F);
			stack.Translate(0.0F, std::numbers::sqrt2_v<float>, 0.0F);
			stack.Rotate({-0.707F, 0.0F, -0.707F}, 54.735F);

			drawObject(tetrahedronMesh_, Shading::VertexColor, MaterialId::Tetrahedron,
			           "lit-color");
		}

		{
			const glc::MatrixStack::Frame monolithFrame = stack.push();

			stack.Translate(88.0F, 5.0F, -80.0F);
			stack.Scale(4.0F);
			stack.Scale(4.0F, 9.0F, 1.0F);
			stack.Translate(0.0F, 0.5F, 0.0F);

			drawObject(cubeMesh_, Shading::Material, MaterialId::Monolith, "lit");
		}

		{
			const glc::MatrixStack::Frame cubeFrame = stack.push();

			stack.Translate(-52.5F, 14.0F, 65.0F);
			stack.RotateZ(50.0F);
			stack.RotateY(-10.0F);
			stack.Scale(20.0F);

			drawObject(cubeMesh_, Shading::VertexColor, MaterialId::Cube, "lit-color");
		}

		{
			const glc::MatrixStack::Frame cylinderFrame = stack.push();

			stack.Translate(-7.0F, 30.0F, -14.0F);
			stack.Scale(15.0F, 55.0F, 15.0F);
			stack.Translate(0.0F, 0.5F, 0.0F);

			drawObject(cylinderMesh_, Shading::VertexColor, MaterialId::Cylinder, "lit-color");
		}

		{
			const glc::MatrixStack::Frame sphereFrame = stack.push();

			stack.Translate(-83.0F, 14.0F, -77.0F);
			stack.Scale(20.0F);

			drawObject(sphereMesh_, Shading::Material, MaterialId::Sphere, "lit");
		}

		drawLightMarkers(stack);
	}

	/// Spheres standing in for the lights, drawn with the unlit program so they
	/// show their own emitted colour rather than being shaded by each other.
	void drawLightMarkers(glc::MatrixStack& stack) {
		if (!drawLights_) {
			return;
		}

		{
			const glc::MatrixStack::Frame sunFrame = stack.push();

			stack.Translate(lightsManager_.sunDirection() * kSunMarkerDistance);
			stack.Scale(kSunMarkerScale);

			drawMarker(lightsManager_.sunIntensity());
		}

		for (std::size_t i = 0; i < kNumberOfPointLights; ++i) {
			const glc::MatrixStack::Frame lightFrame = stack.push();

			stack.Translate(lightsManager_.pointLightPosition(i));
			stack.Scale(kPointMarkerScale);

			drawMarker(lightsManager_.pointLightIntensity(i));
		}
	}

	/// Separate from drawObject because the unlit program has no normal matrix,
	/// and Program::set throws on a uniform the linker dropped.
	void drawMarker(const glm::vec4& color) {
		const glc::Program& program = unlitProgram_->get();
		program.use();
		program.set("modelToCameraMatrix", matrices().Top());
		program.set("objectColor", color);
		sphereMesh_.render("flat");
	}

private:
	glc::FlyController fly_{{-100.0F, 100.0F, 160.0F}, -66.0F, -25.5F};

	LightManager lightsManager_;
	glc::CycleTimer tetraTimer_{kTetraSpinSeconds};
	MaterialSet materials_;

	glc::ReloadableProgram* unlitProgram_{};
	glc::ReloadableProgram* litVertexColorProgram_{};
	glc::ReloadableProgram* litMaterialProgram_{};

	bool drawLights_{true};

	glc::UniformBuffer projectionBlock_{glc::UniformBuffer::forType<ProjectionBlock>()};
	glc::UniformBuffer lightBlock_{glc::UniformBuffer::forType<LightBlock>()};

	glc::Mesh groundMesh_;
	glc::Mesh tetrahedronMesh_;
	glc::Mesh cubeMesh_;
	glc::Mesh cylinderMesh_;
	glc::Mesh sphereMesh_;

	/// A pause checkbox and a pair of scrub buttons for one set of clocks.
	/// ImGui keeps no state of its own, so the checkbox has to be seeded from
	/// the manager every frame -- otherwise a keyboard shortcut would leave the
	/// widget disagreeing with the timer it controls.
	void drawTimerRow(const char* label, TimerScope scope) {
		ImGui::PushID(label);

		bool paused = lightsManager_.isPaused(scope);
		if (ImGui::Checkbox(label, &paused)) {
			lightsManager_.setPaused(scope, paused);
		}

		ImGui::SameLine();
		if (ImGui::SmallButton("-1s")) {
			lightsManager_.rewind(scope, kScrubSeconds);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("+1s")) {
			lightsManager_.fastForward(scope, kScrubSeconds);
		}

		ImGui::PopID();
	}

	void drawTimeControls() {
		const float clockHours = std::fmod(lightsManager_.sunTime() + kClockOffsetHours, 24.0F);
		const auto hour = static_cast<int>(clockHours);
		const auto minute = static_cast<int>((clockHours - static_cast<float>(hour)) * 60.0F);

		ImGui::Text("%02d:%02d", hour, minute);

		// Seeded from the manager every frame, so it tracks the running clock and
		// becomes a scrub handle the moment it is dragged.
		float scrubHours = clockHours;
		if (ImGui::SliderFloat("##time", &scrubHours, 0.0F, 24.0F, "%05.2f h")) {
			lightsManager_.setSunTime(std::fmod(scrubHours - kClockOffsetHours + 24.0F, 24.0F));
		}

		drawTimerRow("Sun", TimerScope::Sun);
		drawTimerRow("Point lights", TimerScope::PointLights);

		if (ImGui::Button("Pause all")) {
			lightsManager_.togglePause(TimerScope::All);
		}
		ImGui::SameLine();
		if (ImGui::Button("-1s all")) {
			lightsManager_.rewind(TimerScope::All, kScrubSeconds);
		}
		ImGui::SameLine();
		if (ImGui::Button("+1s all")) {
			lightsManager_.fastForward(TimerScope::All, kScrubSeconds);
		}
	}

	void drawLightingControls() {
		ImGui::Checkbox("Show light markers", &drawLights_);

		float halfDistance = lightsManager_.halfBrightnessDistance();
		if (ImGui::SliderFloat("Half-brightness distance", &halfDistance, 5.0F, 500.0F, "%.1f",
		                       ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat)) {
			lightsManager_.setHalfBrightnessDistance(halfDistance);
		}
	}

	void drawCameraControls() {
		const glm::vec3& position = fly_.position();
		ImGui::Text("position  %7.1f %7.1f %7.1f", static_cast<double>(position.x),
		            static_cast<double>(position.y), static_cast<double>(position.z));
		ImGui::Text("yaw %.1f deg   pitch %.1f deg", static_cast<double>(fly_.yaw()),
		            static_cast<double>(fly_.pitch()));
		ImGui::Text("fov %.0f   near %.1f   far %.0f", static_cast<double>(camera().fovY()),
		            static_cast<double>(camera().zNear()), static_cast<double>(camera().zFar()));

		ImGui::SliderFloat("Speed", &fly_.settings().unitsPerSecond, 1.0F, 400.0F, "%.0f u/s",
		                   ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat);

		// Fly somewhere worth keeping, then paste the result into the member
		// initializer. Beats transcribing six numbers off the screen by hand.
		if (ImGui::Button("Copy as FlyController{...}")) {
			ImGui::SetClipboardText(
			    std::format("glc::FlyController fly_{{{{{:.1f}F, {:.1f}F, {:.1f}F}}, {:.1f}F, "
				            "{:.1f}F}};",
				            position.x, position.y, position.z, fly_.yaw(), fly_.pitch())
			        .c_str());
		}
	}

	/// Binds the material slice, picks and activates the matching program, and
	/// draws.
	///
	/// Shading selects the program rather than the caller passing one, because
	/// the two can disagree in a way that produces no error: the vertex-colour
	/// program run over a "lit" VAO reads the disabled-attribute constant
	/// (0, 0, 0, 1) and multiplies the material to black.
	///
	/// Program::set throws in debug builds when the program is not the bound
	/// one, so use() has to happen here rather than once per frame.
	void drawObject(const glc::Mesh& mesh, Shading shading, MaterialId materialId,
	                std::optional<std::string_view> vaoName = std::nullopt) {
		materials_.bind(kMaterialBlockBinding, materialId);

		const glc::Program& program = shading == Shading::VertexColor
		                                  ? litVertexColorProgram_->get()
		                                  : litMaterialProgram_->get();
		program.use();

		// Non-uniform scales in this scene (the monolith is 4x9x1), so the
		// upper-left 3x3 is no longer enough -- normals need the inverse
		// transpose to stay perpendicular.
		const glm::mat3 normalModelToCamera(
		    glm::transpose(glm::inverse(glm::mat3(matrices().Top()))));

		program.set("modelToCameraMatrix", matrices().Top());
		program.set("normalModelToCameraMatrix", normalModelToCamera);

		if (!vaoName.has_value()) {
			mesh.render();
			return;
		}
		mesh.render(vaoName.value());
	}
};

} // namespace

int main() {
	return glc::runApp<DynamicRange>();
}
