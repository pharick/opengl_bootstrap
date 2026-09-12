#include "glcore/app.hpp"
#include "glcore/camera.hpp"
#include "glcore/mesh.hpp"
#include "glcore/paths.hpp"
#include "glcore/uniform_buffer.hpp"

#include <array>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <numbers>
#include <optional>
#include <string_view>

namespace {

/* Projection Block */

struct ProjectionBlock {
	glm::mat4 cameraToClipMatrix;
};
static_assert(sizeof(ProjectionBlock) == 64);
constexpr GLuint kProjectionBlockBinding = 0;

/* Light Block */

struct PerLight {
	glm::vec4 cameraSpacePos;
	glm::vec4 intensity;
};

constexpr int kNumberOfLights = 4;

struct LightBlock {
	glm::vec4 ambientIntensity;
	float lightAttenuation;
	std::array<float, 3> padding; // std140 requires vec4 alignment
	std::array<PerLight, kNumberOfLights> lights;
};
static_assert(sizeof(LightBlock) == 160);
constexpr GLuint kLightBlockBinding = 1;

/* Application */

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

		lightsData_ = {
		    .ambientIntensity = glm::vec4{0.2F, 0.2F, 0.2F, 1.0F},
		    .lightAttenuation = 1.0F / (70.0F * 70.0F), // half-brightness distance at 70 units
		    .lights =
		        {
		            // Directional light
		            PerLight{
		                .cameraSpacePos = glm::vec4{0.0F, 0.6F, 0.8F, 0.0F},
		                .intensity = glm::vec4{0.6F, 0.6F, 0.6F, 1.0F},
		            },
		            // Point lights
		            PerLight{
		                .cameraSpacePos = glm::vec4{-50.0F, 30.0F, 70.0F, 1.0F},
		                .intensity = glm::vec4{0.2F, 0.2F, 0.2F, 1.0F},
		            },
		            PerLight{
		                .cameraSpacePos = glm::vec4{70.0F, 30.0F, 50.0F, 1.0F},
		                .intensity = glm::vec4{0.0F, 0.0F, 0.3F, 1.0F},
		            },
		            PerLight{
		                .cameraSpacePos = glm::vec4{50.0F, 30.0F, -70.0F, 1.0F},
		                .intensity = glm::vec4{0.3F, 0.0F, 0.0F, 1.0F},
		            },
		        },
		};

		fly_.settings().unitsPerSecond = 50.0F;
	}

	void onResize(int /*width*/, int /*height*/) override {
		projectionBlock_.update(ProjectionBlock{camera().projection()});
	}

	void onUpdate(float deltaSeconds) override {
		fly_.update(camera(), input(), deltaSeconds);
	}

	void onRender() override {
		lightBlock_.update(lightBlock());

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
	LightBlock lightsData_{};

	glc::ReloadableProgram* program_{};

	glc::UniformBuffer projectionBlock_{glc::UniformBuffer::forType<ProjectionBlock>()};
	glc::UniformBuffer lightBlock_{glc::UniformBuffer::forType<LightBlock>()};

	glc::Mesh groundMesh_;
	glc::Mesh tetrahedronMesh_;
	glc::Mesh cubeMesh_;
	glc::Mesh cylinderMesh_;
	glc::Mesh sphereMesh_;

	[[nodiscard]] LightBlock lightBlock() const {
		LightBlock result = lightsData_;
		for (PerLight& light : result.lights) {
			const glm::vec4 pos = camera().view() * light.cameraSpacePos;
			light.cameraSpacePos = pos;
		}
		return result;
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
