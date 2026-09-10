// Tutorial 11 (Shinies): specular highlights.
//
// Everything up to Chapter 10 was view-independent. Diffuse light scatters
// equally in all directions, so a Lambertian surface looks the same wherever you
// stand -- move the camera and the shading does not change at all. Specular is
// the first term that cares where the viewer is: it is the concentrated,
// mirror-like bounce, so it lands in one place for you and somewhere else for
// the person beside you. Orbit the camera with the light held still and the
// highlight slides across the cylinder.
//
// Three models are selectable, because Phong -- the obvious one -- has a real
// failure mode, and the other two are the fixes people reached for. See
// assets/shaders/common/specular.glsl for what each one measures. To see the
// difference: display "Specular only", shininess down around 1.5, camera pitched
// almost level with the ground plane, and watch the edge of the pool of light.

#include <glcore/app.hpp>
#include <glcore/mesh.hpp>
#include <glcore/paths.hpp>
#include <glcore/uniform_buffer.hpp>

#include <glm/ext/scalar_constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <imgui.h>

#include <cmath>
#include <cstdint>

namespace {

struct ProjectionBlock {
	glm::mat4 cameraToClip;
};
static_assert(sizeof(ProjectionBlock) == 64);

constexpr GLuint kProjectionBlockBinding = 0;

// gltut's UnitPlane is a unit quad and its UnitCylinder is centred on the
// origin, spanning y = -0.5 .. +0.5 -- hence the lift, so it stands on the
// ground rather than being half buried in it.
constexpr float kGroundScale = 8.0F;
constexpr float kCylinderLift = 0.5F;
constexpr float kLightMarkerScale = 0.1F;
constexpr float kLightOrbitDegreesPerSecond = 30.0F;

constexpr glm::vec3 kGroundColor{0.9F, 0.9F, 0.9F};
constexpr glm::vec3 kCylinderColor{0.7F, 0.7F, 0.9F};

/// Values must match the kModel* constants at the top of per_fragment.frag.
enum class SpecularModel : std::uint8_t { Phong = 0, BlinnPhong = 1, Gaussian = 2 };

/// Which terms of the lighting equation to display. Isolating the specular term
/// is the only way to see its artifacts -- diffuse otherwise covers them.
/// Values must match the kLighting* constants in per_fragment.frag.
enum class LightingMode : std::uint8_t { All = 0, DiffuseOnly = 1, SpecularOnly = 2 };

struct LightSettings {
	glm::vec3 intensity{1.0F};
	glm::vec3 ambientIntensity{0.0F};

	float orbitAngle{glm::radians(45.0F)}; ///< radians, as SliderAngle stores it
	float orbitRadius{3.0F};
	float height{1.0F};
	bool orbiting{true};

	/// Distance at which the lamp falls to half brightness, in world units. A
	/// more usable knob than the raw constant, which is derived from it.
	float halfBrightnessDistance{2.0F};

	/// Physically correct falloff. Looks worse than the linear one with output
	/// clamped to [0,1] and no tone mapping: a blown-out core ringed by black.
	bool inverseSquared{false};

	[[nodiscard]] glm::vec3 position() const {
		return {orbitRadius * std::cos(orbitAngle), height, orbitRadius * std::sin(orbitAngle)};
	}

	/// k in `I / (1 + k * d)`, chosen so that d == halfBrightnessDistance halves
	/// the intensity under whichever curve is selected.
	[[nodiscard]] float attenuation() const {
		if (inverseSquared) {
			return 1.0F / (halfBrightnessDistance * halfBrightnessDistance);
		}
		return 1.0F / halfBrightnessDistance;
	}
};

struct MaterialSettings {
	/// Specular is the bounce off the surface itself, before any pigment absorbs
	/// anything -- which is why a red plastic ball has a white highlight and this
	/// is a separate colour from the diffuse one.
	glm::vec3 specularColor{0.25F};

	/// Phong / Blinn exponent. Bigger is tighter. Not interchangeable between
	/// those two: Blinn needs roughly 4x Phong's for the same highlight size.
	float shininess{30.0F};

	/// Gaussian's angular spread, in radians. A different quantity from the
	/// exponent above, with a range that does not overlap it at all.
	float gaussianWidth{0.2F};

	SpecularModel model{SpecularModel::BlinnPhong};
};

/// The per-object half of the uniform set, which is otherwise three identical
/// lines at every draw.
///
/// The normal matrix is just the upper-left 3x3: every transform in this scene
/// is a rotation or a uniform scale, and neither needs the inverse transpose
/// that tut09 introduced. Add a non-uniform scale and that stops being true.
void setObjectUniforms(const glc::Program& program, const glm::mat4& modelToCamera,
                       const glm::vec3& diffuseColor) {
	program.set("modelToCamera", modelToCamera);
	program.set("normalModelToCamera", glm::mat3{modelToCamera});
	program.set("diffuseColor", diffuseColor);
}

class SpecularLights final : public glc::App {
public:
	SpecularLights()
	    : glc::App({
	          .window = {.title = "gltut 11 -- Specular Lights"},
	          .depthTest = true,
	          .cullFace = true,
	          .frontFace = GL_CW, // gltut's meshes are wound clockwise
	      }) {}

protected:
	void onInit() override {
		program_ = &shaders().add(glc::paths::tutorialShader("per_fragment.vert"),
		                          glc::paths::tutorialShader("per_fragment.frag"));

		planeMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitPlane.xml"));
		cylinderMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitCylinder.xml"));
		lightMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitCube.xml"));

		projectionBlock_.bindToPoint(kProjectionBlockBinding);
		program_->get().bindUniformBlock("Projection", kProjectionBlockBinding);

		camera().setPerspective(45.0F, aspect(), 0.1F, 100.0F);
		projectionBlock_.update(ProjectionBlock{camera().projection()});
	}

	void onResize(int /*width*/, int /*height*/) override {
		projectionBlock_.update(ProjectionBlock{camera().projection()});
	}

	void onUpdate(float deltaSeconds) override {
		orbit_.update(camera(), input());
		if (light_.orbiting) {
			light_.orbitAngle += deltaSeconds * glm::radians(kLightOrbitDegreesPerSecond);
			// Keeps the value small enough that float precision stays constant,
			// and inside the slider's range so the handle keeps tracking.
			light_.orbitAngle = glm::mod(light_.orbitAngle, glm::two_pi<float>());
		}
	}

	void onRender() override {
		const glc::Program& program = program_->get();
		program.use();

		uploadLightUniforms(program);
		uploadMaterialUniforms(program);

		glc::MatrixStack& stack = matrices();
		stack.SetMatrix(camera().view());

		{
			const glc::MatrixStack::Frame ground = stack.push();
			stack.Scale(kGroundScale);
			setObjectUniforms(program, stack.Top(), kGroundColor);
			planeMesh_.render();
		}

		{
			const glc::MatrixStack::Frame body = stack.push();
			stack.Translate(0.0F, kCylinderLift, 0.0F);
			stack.RotateX(cylinderRotation_);
			setObjectUniforms(program, stack.Top(), kCylinderColor);
			// The "lit" VAO is position + normal; the file's vertex colours are
			// not wanted, since flat grey shows shading gradients better.
			cylinderMesh_.render("lit");
		}

		drawLightMarker(program, stack);
	}

	void onGui() override {
		if (ImGui::Begin("Tutorial 11")) {
			if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawDisplayControls();
			}
			if (ImGui::CollapsingHeader("Scene")) {
				ImGui::SliderFloat("Cylinder rotation", &cylinderRotation_, 0.0F, 360.0F,
				                   "%.0f deg");
			}
			if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawLightControls();
			}
			if (ImGui::CollapsingHeader("Attenuation")) {
				drawAttenuationControls();
			}
			if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawMaterialControls();
			}
		}
		ImGui::End();
	}

private:
	void uploadLightUniforms(const glc::Program& program) const {
		// The light is authored in world space but the shader works in camera
		// space. w = 1 because this is a point, not a direction -- translation
		// has to apply to it.
		const glm::vec3 cameraSpacePosition = camera().view() * glm::vec4{light_.position(), 1.0F};

		program.set("lightPosition", cameraSpacePosition);
		program.set("lightIntensity", light_.intensity);
		program.set("ambientIntensity", light_.ambientIntensity);
		program.set("lightAttenuation", light_.attenuation());
		program.set("useInverseSquaredAttenuation", light_.inverseSquared);
	}

	void uploadMaterialUniforms(const glc::Program& program) const {
		program.set("specularColor", material_.specularColor);
		program.set("shininessFactor", material_.shininess);
		program.set("gaussianWidth", material_.gaussianWidth);
		program.set("specularModel", static_cast<int>(material_.model));
		program.set("lightingMode", static_cast<int>(lightingMode_));
	}

	/// A small cube standing in for the lamp, drawn unlit: zero light intensity
	/// with ambient at 1 leaves the shader emitting exactly diffuseColor, which
	/// is set to the light's own colour so the marker shows what it is emitting.
	///
	/// Uniforms belong to the program, not to this scope, so both are put back on
	/// the way out. Without that the scene stays lit only because onRender
	/// happens to re-upload them first -- an invisible dependency on draw order.
	void drawLightMarker(const glc::Program& program, glc::MatrixStack& stack) const {
		const glc::MatrixStack::Frame marker = stack.push();
		stack.Translate(light_.position());
		stack.Scale(kLightMarkerScale);

		program.set("lightIntensity", glm::vec3{0.0F});
		program.set("ambientIntensity", glm::vec3{1.0F});
		setObjectUniforms(program, stack.Top(), light_.intensity);
		lightMesh_.render();

		program.set("lightIntensity", light_.intensity);
		program.set("ambientIntensity", light_.ambientIntensity);
	}

	void drawDisplayControls() {
		int mode = static_cast<int>(lightingMode_);
		if (ImGui::Combo("Terms", &mode, "Diffuse + specular\0Diffuse only\0Specular only\0")) {
			lightingMode_ = static_cast<LightingMode>(mode);
		}

		int model = static_cast<int>(material_.model);
		if (ImGui::Combo("Specular model", &model, "Phong\0Blinn-Phong\0Gaussian\0")) {
			material_.model = static_cast<SpecularModel>(model);
		}
	}

	void drawLightControls() {
		ImGui::ColorEdit3("Intensity", glm::value_ptr(light_.intensity));
		ImGui::ColorEdit3("Ambient", glm::value_ptr(light_.ambientIntensity));
		ImGui::SliderAngle("Orbit angle", &light_.orbitAngle, 0.0F, 360.0F);
		ImGui::SliderFloat("Orbit radius", &light_.orbitRadius, 0.0F, 10.0F);
		ImGui::SliderFloat("Height", &light_.height, 0.0F, 10.0F);
		ImGui::Checkbox("Orbiting", &light_.orbiting);
	}

	void drawAttenuationControls() {
		ImGui::SliderFloat("Half-brightness distance", &light_.halfBrightnessDistance, 0.5F, 50.0F,
		                   "%.1f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat);
		ImGui::Checkbox("Inverse squared", &light_.inverseSquared);
	}

	void drawMaterialControls() {
		ImGui::ColorEdit3("Specular color", glm::value_ptr(material_.specularColor));

		// Only one of these is meaningful at a time. Showing both would invite
		// dragging an exponent into a slider that wants radians.
		if (material_.model == SpecularModel::Gaussian) {
			ImGui::SliderFloat("Gaussian width", &material_.gaussianWidth, 0.02F, 1.0F, "%.3f rad",
			                   ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat);
		} else {
			ImGui::SliderFloat("Shininess", &material_.shininess, 1.0F, 300.0F, "%.1f",
			                   ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat);
		}
	}

	glc::ReloadableProgram* program_{};

	glc::Mesh planeMesh_;
	glc::Mesh cylinderMesh_;
	glc::Mesh lightMesh_;

	glc::UniformBuffer projectionBlock_{glc::UniformBuffer::forType<ProjectionBlock>()};
	glc::OrbitController orbit_{glm::vec3{0.0F}, 5.0F};

	LightSettings light_;
	MaterialSettings material_;
	LightingMode lightingMode_{LightingMode::All};

	/// Degrees. Tipping the cylinder presents every surface orientation at once,
	/// which beats sweeping the light and trying to remember the last frame.
	float cylinderRotation_{0.0F};
};

} // namespace

int main() {
	return glc::runApp<SpecularLights>();
}
