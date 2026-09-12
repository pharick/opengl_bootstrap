#include "glcore/app.hpp"
#include "glcore/camera.hpp"
#include "glcore/mesh.hpp"
#include "glcore/paths.hpp"
#include "glcore/uniform_buffer.hpp"

#include "lights.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>

#include <imgui.h>

#include <cmath>
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

struct ProjectionBlock {
	glm::mat4 cameraToClipMatrix;
};
static_assert(sizeof(ProjectionBlock) == 64);

constexpr GLuint kProjectionBlockBinding = 0;
constexpr GLuint kLightBlockBinding = 1;

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
		program_ = &shaders().add(glc::paths::tutorialShader("basic.vert"),
		                          glc::paths::tutorialShader("basic.frag"));

		groundMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/Ground.xml"));
		tetrahedronMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitTetrahedron.xml"));
		cubeMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitCubeLit.xml"));
		cylinderMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitCylinder.xml"));
		sphereMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitSphere.xml"));

		projectionBlock_.bindToPoint(kProjectionBlockBinding);
		program_->get().bindUniformBlock("Projection", kProjectionBlockBinding);

		camera().setPerspective(45.0F, aspect(), 1.0F, 1000.0F);
		projectionBlock_.update(ProjectionBlock{camera().projection()});

		lightBlock_.bindToPoint(kLightBlockBinding);
		program_->get().bindUniformBlock("Light", kLightBlockBinding);

		fly_.settings().unitsPerSecond = 50.0F;
	}

	void onResize(int /*width*/, int /*height*/) override {
		projectionBlock_.update(ProjectionBlock{camera().projection()});
	}

	void onUpdate(float deltaSeconds) override {
		fly_.update(camera(), input(), deltaSeconds);
		lightsManager_.update(deltaSeconds);
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

		const glc::Program& program = program_->get();
		program.use();

		glc::MatrixStack& stack = matrices();
		stack.SetMatrix(camera().view());

		{
			const glc::MatrixStack::Frame groundFrame = stack.push();

			stack.RotateX(-90.0F);

			drawObject(groundMesh_);
		}

		{
			const glc::MatrixStack::Frame tetrahedronFrame = stack.push();

			stack.Translate(75.0F, 5.0F, 75.0F);
			stack.Scale(10.0F);
			stack.Translate(0.0F, std::numbers::sqrt2_v<float>, 0.0F);
			stack.Rotate({-0.707F, 0.0F, -0.707F}, 54.735F);

			drawObject(tetrahedronMesh_, "lit-color");
		}

		{
			const glc::MatrixStack::Frame monolithFrame = stack.push();

			stack.Translate(88.0F, 5.0F, -80.0F);
			stack.Scale(4.0F);
			stack.Scale(4.0F, 9.0F, 1.0F);
			stack.Translate(0.0F, 0.5F, 0.0F);

			drawObject(cubeMesh_, "lit");
		}

		{
			const glc::MatrixStack::Frame cubeFrame = stack.push();

			stack.Translate(-52.5F, 14.0F, 65.0F);
			stack.RotateZ(50.0F);
			stack.RotateY(-10.0F);
			stack.Scale(20.0F);

			drawObject(cubeMesh_, "lit-color");
		}

		{
			const glc::MatrixStack::Frame cylinderFrame = stack.push();

			stack.Translate(-7.0F, 30.0F, -14.0F);
			stack.Scale(15.0F, 55.0F, 15.0F);
			stack.Translate(0.0F, 0.5F, 0.0F);

			drawObject(cylinderMesh_, "lit-color");
		}

		{
			const glc::MatrixStack::Frame sphereFrame = stack.push();

			stack.Translate(-83.0F, 14.0F, -77.0F);
			stack.Scale(20.0F);

			drawObject(sphereMesh_, "lit");
		}
	}

private:
	glc::FlyController fly_{{-59.5F, 79.0F, 130.0F}, -90.0F, -45.0F};
	LightManager lightsManager_;

	glc::ReloadableProgram* program_{};

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
		ImGui::ProgressBar(lightsManager_.sunAlpha(), ImVec2{-1.0F, 0.0F});

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

	void drawObject(const glc::Mesh& mesh, std::optional<std::string_view> vaoName = std::nullopt) {
		const glc::Program& program = program_->get();

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
