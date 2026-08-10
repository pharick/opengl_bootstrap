// A reference tutorial exercising the pieces the book needs from Chapter 6 on:
// an XML mesh, the orbit camera, the matrix stack and an ImGui panel.
//
// Controls: left-drag orbits, shift+left or middle-drag pans, scroll zooms.

#include <glcore/app.hpp>
#include <glcore/mesh.hpp>
#include <glcore/paths.hpp>

#include <imgui.h>

namespace {

class CubeAndCamera final : public glc::App {
public:
	CubeAndCamera()
	    : glc::App({.window = {.title = "gltut 02 -- Cube and Camera"}, .cullFace = false}) {}

protected:
	void onInit() override {
		program_ = &shaders().add(glc::paths::tutorialShader("cube.vert"),
		                          glc::paths::tutorialShader("cube.frag"));

		mesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitCube.xml"));
		glc::log::info("cube VAOs: {}", mesh_.vaoNames().size());

		camera().setPerspective(45.0F, aspect(), 0.1F, 100.0F);
	}

	void onUpdate(float deltaSeconds) override {
		orbit_.update(camera(), input());
		if (spin_) {
			spinDegrees_ += deltaSeconds * 40.0F;
		}
	}

	void onRender() override {
		const glc::Program& program = program_->get();
		program.use();

		glc::MatrixStack& stack = matrices();
		stack.SetMatrix(camera().viewProjection());

		// One cube at the origin, plus a smaller child orbiting it -- the
		// parent/child composition the matrix stack exists for.
		{
			const glc::MatrixStack::Frame body = stack.push();
			stack.RotateY(spinDegrees_);
			program.set("modelToClip", stack.Top());
			mesh_.render();

			{
				const glc::MatrixStack::Frame satellite = stack.push();
				stack.Translate(1.6F, 0.0F, 0.0F);
				stack.RotateZ(spinDegrees_ * 2.0F);
				stack.Scale(0.35F);
				program.set("modelToClip", stack.Top());
				mesh_.render();
			}
		}
	}

	void onGui() override {
		if (ImGui::Begin("Tutorial 02")) {
			ImGui::Checkbox("Spin", &spin_);
			ImGui::SliderFloat("Angle", &spinDegrees_, 0.0F, 360.0F);
			ImGui::Separator();
			ImGui::Text("distance %.2f", static_cast<double>(orbit_.distance()));
			ImGui::Text("yaw %.1f  pitch %.1f", static_cast<double>(orbit_.yaw()),
			            static_cast<double>(orbit_.pitch()));
		}
		ImGui::End();
	}

private:
	glc::ReloadableProgram* program_ = nullptr;
	glc::Mesh mesh_;
	glc::OrbitController orbit_{glm::vec3{0.0F}, 5.0F};
	float spinDegrees_ = 0.0F;
	bool spin_ = true;
};

} // namespace

int main() {
	return glc::runApp<CubeAndCamera>();
}
