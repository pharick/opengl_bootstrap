// gltut Tutorial 1 -- Hello, Triangle!
//
// The smallest complete glcore program: one buffer, one VAO, one program.

#include <glcore/app.hpp>
#include <glcore/buffer.hpp>
#include <glcore/paths.hpp>
#include <glcore/scoped_bind.hpp>
#include <glcore/vertex_array.hpp>

#include <array>

namespace {

/// Kept next to the vertex data so it stays in step with the shader's
/// `layout(location = 0)`.
constexpr GLuint kPositionLocation = 0;

constexpr std::array<float, 9> kTriangle{
    // clang-format off
	 0.00F,  0.60F, 0.0F,
	 0.55F, -0.45F, 0.0F,
	-0.55F, -0.45F, 0.0F,
    // clang-format on
};

class HelloTriangle final : public glc::App {
public:
	HelloTriangle()
	    : glc::App({.window = {.title = "gltut 01 -- Hello, Triangle!"}, .depthTest = false}) {}

protected:
	void onInit() override {
		// Registering with the watcher instead of building a Program directly
		// means editing either shader rebuilds it while the app runs.
		program_ = &shaders().add(glc::paths::tutorialShader("tut01.vert"),
		                          glc::paths::tutorialShader("tut01.frag"));

		vbo_ = glc::makeBuffer(GL_ARRAY_BUFFER, kTriangle);
		vao_ = glc::makeVertexArray(
		    vbo_, std::array{glc::AttributeDesc{.location = kPositionLocation, .components = 3}});
	}

	void onRender() override {
		program_->get().use();
		const glc::ScopedBind bind{vao_};
		GLC_CHECK(glDrawArrays(GL_TRIANGLES, 0, 3));
	}

private:
	// Owned by the App's ShaderWatcher, which outlives onRender.
	glc::ReloadableProgram* program_ = nullptr;
	glc::Buffer vbo_;
	glc::VertexArray vao_;
};

} // namespace

int main() {
	return glc::runApp<HelloTriangle>();
}
