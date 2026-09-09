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
	      }) {}

protected:
	void onInit() override {
		program_ = &shaders().add(glc::paths::tutorialShader("shader.vert"),
		                          glc::paths::tutorialShader("shader.frag"));

		planeMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitPlane.xml"));

		projectionBlock_.bindToPoint(kProjectionBlockBinding);
		program_->get().bindUniformBlock("Projection", kProjectionBlockBinding);

		camera().setPerspective(45.0F, aspect(), 0.1F, 100.0F);
		projectionBlock_.update(ProjectionBlock{camera().projection()});
	}

	void onResize(int /*width*/, int /*height*/) override {
		projectionBlock_.update(ProjectionBlock{camera().projection()});
	}

	void onUpdate(float /*deltaSeconds*/) override {
		orbit_.update(camera(), input());
	}

	void onRender() override {
		const glc::Program& program = program_->get();
		program.use();

		const glm::vec3 lightDirCameraSpace =
		    glm::vec3{camera().view() * glm::vec4{lightDirection(), 0.0}};

		program.set("diffuseColor", glm::vec3{0.9F, 0.9F, 0.9F});
		program.set("dirToLight", lightDirCameraSpace);
		program.set("lightIntensity", lightIntensity_);
		program.set("ambientIntensity", ambientIntensity_);

		glc::MatrixStack& stack = matrices();
		stack.SetMatrix(camera().view());

		{
			const glc::MatrixStack::Frame body = stack.push();
			stack.Scale(8.0F);

			program.set("modelToCamera", stack.Top());

			const glm::mat3 normalModelToCamera(stack.Top());
			program.set("normalModelToCamera", normalModelToCamera);

			planeMesh_.render();
		}
	}

	void onGui() override {
		if (ImGui::Begin("Light Source")) {
			ImGui::SliderAngle("Azimuth", &lightAzimuth_, -180.0F, 180.0F);
			ImGui::SliderAngle("Elevation", &lightElevation_, -90.0F, 90.F);

			const glm::vec3 dir = lightDirection();
			ImGui::Text("dir %.3f %.3f %.3f", static_cast<double>(dir.x),
			            static_cast<double>(dir.y), static_cast<double>(dir.z));
			ImGui::Text("length  %.4f", static_cast<double>(glm::length(dir)));

			ImGui::ColorEdit3("Intensity", glm::value_ptr(lightIntensity_));
			ImGui::ColorEdit3("Ambient Intensity", glm::value_ptr(ambientIntensity_));
		}
		ImGui::End();
	}

private:
	glc::ReloadableProgram* program_{};

	glc::Mesh planeMesh_;
	glc::Mesh cylinderMesh_;

	glc::UniformBuffer projectionBlock_{glc::UniformBuffer::forType<ProjectionBlock>()};

	glc::OrbitController orbit_{glm::vec3{0.0F}, 5.0F};

	float lightAzimuth_ = 0.0F;
	float lightElevation_ = glm::radians(30.0F);
	glm::vec3 lightIntensity_{1.0F};
	glm::vec3 ambientIntensity_{0.0F};

	[[nodiscard]] glm::vec3 lightDirection() const {
		return {std::cos(lightElevation_) * std::cos(lightAzimuth_), std::sin(lightElevation_),
		        std::cos(lightElevation_) * std::sin(lightAzimuth_)};
	}
};

} // namespace

int main() {
	return glc::runApp<PointLights>();
}
