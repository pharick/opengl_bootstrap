#include "glcore/app.hpp"
#include "glcore/mesh.hpp"
#include "glcore/paths.hpp"
#include "glcore/uniform_buffer.hpp"

#include <cmath>
#include <cstddef>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <imgui.h>

namespace {

struct ProjectionBlock {
	glm::mat4 cameraToClip;
};
static_assert(sizeof(ProjectionBlock) == 64);

constexpr GLuint kProjectionBlockBinding = 0;

class PointLights final : public glc::App {
public:
	PointLights()
	    : glc::App({
	          .window = {.title = "gltut 10 -- Point Lights"},
	          .depthTest = true,
	          .cullFace = true,
	          .frontFace = GL_CW, // gltut's meshes are wound clockwise
	      }) {}

protected:
	void onInit() override {
		perVertexProgram_ = &shaders().add(glc::paths::tutorialShader("per_vertex.vert"),
		                                   glc::paths::tutorialShader("per_vertex.frag"));
		perFragmentProgram_ = &shaders().add(glc::paths::tutorialShader("per_fragment.vert"),
		                                     glc::paths::tutorialShader("per_fragment.frag"));

		planeMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitPlane.xml"));
		cylinderMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitCylinder.xml"));
		lightMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitCube.xml"));

		projectionBlock_.bindToPoint(kProjectionBlockBinding);

		perVertexProgram_->get().bindUniformBlock("Projection", kProjectionBlockBinding);
		perFragmentProgram_->get().bindUniformBlock("Projection", kProjectionBlockBinding);

		camera().setPerspective(45.0F, aspect(), 0.1F, 100.0F);
		projectionBlock_.update(ProjectionBlock{camera().projection()});
	}

	void onResize(int /*width*/, int /*height*/) override {
		projectionBlock_.update(ProjectionBlock{camera().projection()});
	}

	void onUpdate(float deltaSeconds) override {
		orbit_.update(camera(), input());
		if (lightSourceOrbiting_) {
			lightSourceOrbitAngle_ += deltaSeconds * glm::radians(30.0F);
			lightSourceOrbitAngle_ = glm::mod(lightSourceOrbitAngle_, glm::two_pi<float>());
		}
	}

	void onRender() override {
		const glc::Program& program =
		    perFragmentLighting_ ? perFragmentProgram_->get() : perVertexProgram_->get();
		program.use();

		const glm::vec3 lightPositionCameraSpace =
		    camera().view() * glm::vec4{lightPosition(), 1.0F};

		program.set("lightPosition", lightPositionCameraSpace);
		program.set("lightIntensity", lightIntensity_);
		program.set("ambientIntensity", ambientIntensity_);
		program.set("lightAttenuation", lightAttenuation());
		program.set("useInverseSquaredAttenuation", useInverseSquaredAttenuation_);

		glc::MatrixStack& stack = matrices();
		stack.SetMatrix(camera().view());

		{
			const glc::MatrixStack::Frame body = stack.push();
			stack.Scale(8.0F);

			program.set("modelToCamera", stack.Top());
			program.set("diffuseColor", glm::vec3{0.9F, 0.9F, 0.9F});

			const glm::mat3 normalModelToCamera(stack.Top());
			program.set("normalModelToCamera", normalModelToCamera);

			planeMesh_.render();
		}

		{
			const glc::MatrixStack::Frame body = stack.push();
			stack.Translate(0.0F, 0.5F, 0.0F);

			program.set("modelToCamera", stack.Top());
			program.set("diffuseColor", glm::vec3{0.7F, 0.7F, 0.9F});

			const glm::mat3 normalModelToCamera(stack.Top());
			program.set("normalModelToCamera", normalModelToCamera);

			cylinderMesh_.render("lit");
		}

		{
			const glc::MatrixStack::Frame body = stack.push();
			stack.Translate(lightPosition());
			stack.Scale(0.1F);

			program.set("modelToCamera", stack.Top());
			program.set("lightIntensity", glm::vec3{0.0F});
			program.set("ambientIntensity", glm::vec3{0.5F});
			program.set("diffuseColor", lightIntensity_);

			const glm::mat3 normalModelToCamera(stack.Top());
			program.set("normalModelToCamera", normalModelToCamera);

			lightMesh_.render();
		}
	}

	void onGui() override {
		if (ImGui::Begin("Light Source")) {
			ImGui::ColorEdit3("Light intensity", glm::value_ptr(lightIntensity_));
			ImGui::ColorEdit3("Ambient intensity", glm::value_ptr(ambientIntensity_));
			ImGui::SliderAngle("Orbit angle", &lightSourceOrbitAngle_, 0.0F, 360.0F);
			ImGui::SliderFloat("Orbit radius", &lightSourceRadius_, 0.0F, 10.0F);
			ImGui::SliderFloat("Height", &lightSourceHeight_, 0.0F, 10.0F);
			ImGui::SliderFloat("Half-brightness distance", &lightAttenuationHalfDistance_, 0.5F,
			                   50.0F, "%.1f",
			                   ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat);
			ImGui::Checkbox("Orbiting", &lightSourceOrbiting_);
			ImGui::Checkbox("Per-fragment lighting", &perFragmentLighting_);
			ImGui::Checkbox("Inverse squared attenuation", &useInverseSquaredAttenuation_);
		}
		ImGui::End();
	}

private:
	glc::ReloadableProgram* perVertexProgram_{};
	glc::ReloadableProgram* perFragmentProgram_{};

	glc::Mesh planeMesh_;
	glc::Mesh cylinderMesh_;
	glc::Mesh lightMesh_;

	glc::UniformBuffer projectionBlock_{glc::UniformBuffer::forType<ProjectionBlock>()};

	glc::OrbitController orbit_{glm::vec3{0.0F}, 5.0F};

	glm::vec3 lightIntensity_{1.0F};
	glm::vec3 ambientIntensity_{0.0F};

	float lightSourceOrbitAngle_{glm::radians(45.0F)};
	float lightSourceRadius_{3.0F};
	float lightSourceHeight_{1.0F};
	bool lightSourceOrbiting_{true};
	bool perFragmentLighting_{true};
	float lightAttenuationHalfDistance_{2.0F};
	bool useInverseSquaredAttenuation_{false};

	[[nodiscard]] glm::vec3 lightPosition() const {
		return glm::vec3{
		    lightSourceRadius_ * std::cos(lightSourceOrbitAngle_),
		    lightSourceHeight_,
		    lightSourceRadius_ * std::sin(lightSourceOrbitAngle_),
		};
	}

	[[nodiscard]] float lightAttenuation() const {
		if (useInverseSquaredAttenuation_) {
			return 1.0F / (lightAttenuationHalfDistance_ * lightAttenuationHalfDistance_);
		}
		return 1.0F / lightAttenuationHalfDistance_;
	}
};

} // namespace

int main() {
	return glc::runApp<PointLights>();
}
