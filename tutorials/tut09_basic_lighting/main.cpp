#include "glcore/app.hpp"
#include "glcore/gl.hpp"
#include "glcore/mesh.hpp"
#include "glcore/paths.hpp"
#include "glcore/uniform_buffer.hpp"

#include <array>
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
#include <span>
#include <vector>

namespace {

struct Vertex {
	glm::vec3 position;
	glm::vec3 color;
	glm::vec3 normal;
};

struct ProjectionBlock {
	glm::mat4 cameraToClip;
};
static_assert(sizeof(ProjectionBlock) == 64);

constexpr GLuint kPositionLocation = 0;
constexpr GLuint kDiffuseColorLocation = 1;
constexpr GLuint kNormalLocation = 2;

constexpr GLuint kProjectionBlockBinding = 0;

constexpr std::array kMeshAttributes{
    glc::AttributeDesc{
        .location = kPositionLocation,
        .components = 3,
        .type = GL_FLOAT,
        .normalized = false,
        .stride = sizeof(Vertex),
        .offset = offsetof(Vertex, position),
    },
    glc::AttributeDesc{
        .location = kDiffuseColorLocation,
        .components = 3,
        .type = GL_FLOAT,
        .normalized = false,
        .stride = sizeof(Vertex),
        .offset = offsetof(Vertex, color),
    },
    glc::AttributeDesc{
        .location = kNormalLocation,
        .components = 3,
        .type = GL_FLOAT,
        .normalized = false,
        .stride = sizeof(Vertex),
        .offset = offsetof(Vertex, normal),
    },
};

constexpr std::array<Vertex, 4> kPlaneVertices = {
    {
        {
            .position = {-0.5F, 0.0F, -0.5F},
            .color = {0.5F, 0.5F, 0.7F},
            .normal = {0.0F, 1.0F, 0.0F},
        },
        {
            .position = {0.5F, 0.0F, -0.5F},
            .color = {0.5F, 0.5F, 0.7F},
            .normal = {0.0F, 1.0F, 0.0F},
        },
        {
            .position = {-0.5F, 0.0F, 0.5F},
            .color = {0.5F, 0.5F, 0.7F},
            .normal = {0.0F, 1.0F, 0.0F},
        },
        {
            .position = {0.5F, 0.0F, 0.5F},
            .color = {0.5F, 0.5F, 0.7F},
            .normal = {0.0F, 1.0F, 0.0F},
        },
    },
};

constexpr std::array<GLuint, 6> kPlaneIndices{
    // clang-format off
	0, 2, 1,
	2, 3, 1,
    // clang-format on
};

float angleForSegment(GLuint segment, GLuint segments) {
	return 2.0F * glm::pi<float>() * static_cast<float>(segment) / static_cast<float>(segments);
}

glc::Mesh makeCylinder(GLuint segments) {
	std::vector<Vertex> vertices;
	std::vector<GLuint> indices;

	constexpr float kRadius = 0.5F;

	// Generate vertices
	for (GLuint i = 0; i <= segments; ++i) {
		const float angle = angleForSegment(i, segments);
		const float x = std::cos(angle);
		const float z = std::sin(angle);

		vertices.push_back(Vertex{
		    .position = {kRadius * x, 0.0F, kRadius * z},
		    .color = {0.5F, 0.5F, 0.7F},
		    .normal = {x, 0.0F, z},
		});
		vertices.push_back(Vertex{
		    .position = {kRadius * x, 1.0F, kRadius * z},
		    .color = {0.5F, 0.5F, 0.7F},
		    .normal = {x, 0.0F, z},
		});
	}

	// Generate indices
	for (GLuint i = 0; i < segments; ++i) {
		const GLuint base = i * 2;
		indices.push_back(base);
		indices.push_back(base + 1);
		indices.push_back(base + 2);

		indices.push_back(base + 1);
		indices.push_back(base + 3);
		indices.push_back(base + 2);
	}

	// Generate top cap vertices
	const auto topCenterIndex = static_cast<GLuint>(vertices.size());
	vertices.push_back(Vertex{
	    .position = {0.0F, 1.0F, 0.0F},
	    .color = {0.4F, 0.4F, 0.6F},
	    .normal = {0.0F, 1.0F, 0.0F},
	});

	const auto topRimBase = static_cast<GLuint>(vertices.size());
	for (GLuint i = 0; i <= segments; ++i) {
		const float angle = angleForSegment(i, segments);
		const float x = std::cos(angle);
		const float z = std::sin(angle);

		vertices.push_back(Vertex{
		    .position = {kRadius * x, 1.0F, kRadius * z},
		    .color = {0.4F, 0.4F, 0.6F},
		    .normal = {0.0F, 1.0F, 0.0F},
		});
	}

	// Generate top cap indices
	for (GLuint i = 0; i < segments; ++i) {
		indices.push_back(topCenterIndex);
		indices.push_back(topRimBase + i + 1);
		indices.push_back(topRimBase + i);
	}

	return glc::Mesh::fromInterleaved(std::as_bytes(std::span{vertices}), kMeshAttributes,
	                                  static_cast<GLsizei>(vertices.size()), indices);
}

class BasicLighting final : public glc::App {
public:
	BasicLighting()
	    : glc::App({
	          .window = {.title = "gltut 09 -- Basic Lighting"},
	          .depthTest = true,
	          .cullFace = true,
	      }) {}

protected:
	void onInit() override {
		planeMesh_ =
		    glc::Mesh::fromInterleaved(std::as_bytes(std::span{kPlaneVertices}), kMeshAttributes,
		                               static_cast<GLsizei>(kPlaneVertices.size()), kPlaneIndices);
		cylinderMesh_ = makeCylinder(16);

		program_ = &shaders().add(glc::paths::tutorialShader("lighting.vert"),
		                          glc::paths::tutorialShader("lighting.frag"));

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

		{
			const glc::MatrixStack::Frame body = stack.push();

			if (scaleCylinder_) {
				stack.Scale(1.0F, 1.0F, 0.2F);
			}

			program.set("modelToCamera", stack.Top());

			const glm::mat3 modelToCamera3{stack.Top()};
			const glm::mat3 normalModelToCamera =
			    useInverseTranspose_ ? glm::inverseTranspose(modelToCamera3) : modelToCamera3;
			program.set("normalModelToCamera", normalModelToCamera);

			cylinderMesh_.render();
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

		if (ImGui::Begin("Normal Transformation")) {
			ImGui::Checkbox("Scale Cylinder", &scaleCylinder_);
			ImGui::Checkbox("Use Inverse Transpose", &useInverseTranspose_);
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

	bool scaleCylinder_ = false;
	bool useInverseTranspose_ = true;

	[[nodiscard]] glm::vec3 lightDirection() const {
		return {std::cos(lightElevation_) * std::cos(lightAzimuth_), std::sin(lightElevation_),
		        std::cos(lightElevation_) * std::sin(lightAzimuth_)};
	}
};

} // namespace

int main() {
	return glc::runApp<BasicLighting>();
}
