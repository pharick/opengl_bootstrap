// Tutorial 14 (Textures are not Pictures): the Perspective Interpolation
// example.
//
// Before a texture can be pinned to a surface, the book stops to ask what
// interpolating a value across a triangle actually means. Linear, yes -- but
// linear in which space? The rasterizer only ever sees window space, and the
// perspective divide that put the triangle there is not linear, so a value
// lerped evenly across pixels is not lerped evenly across the surface. The
// default qualifier, `smooth`, corrects for this using clip-space W; the
// rarely-used `noperspective` does not, and this is the one chapter where the
// difference is the exhibit.
//
// The scene is a corridor of three walls, red at the near end and green at
// the far end, seen from a fixed camera. P switches the qualifier and the
// difference is immediate. S swaps in the "faux" hallway: the same corridor
// flattened onto its near plane, an optical illusion with the same outline
// and the same colors in the same places but no depth. Every vertex of it
// leaves the vertex shader with the same clip-space W, so the correction has
// nothing to key on, and now P does nothing -- every mode looks like the
// linear one.
//
// Two things the book does not have. A third program does the correction by
// hand (by_hand.vert): it lerps color/W and 1/W with `noperspective` and
// divides in the fragment shader, which is what the hardware does for
// `smooth`, spelled out. And the number of depth slices in the corridor is a
// slider: linear interpolation is only wrong *within* a triangle, so more and
// smaller triangles pull the window-space version toward the correct one --
// which is how hardware without perspective correction got by.

#include <glcore/app.hpp>
#include <glcore/gl_check.hpp>
#include <glcore/mesh.hpp>
#include <glcore/paths.hpp>
#include <glcore/vertex_array.hpp>

#include <glm/common.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <imgui.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

constexpr GLuint kPositionLocation = 0;
constexpr GLuint kColorLocation = 1;

/// The book's projection: 60 degrees, near 1, far 1000. The camera is fixed
/// at the origin looking down -Z; the faux hallway is only an illusion from
/// this one spot, so there are no camera controls.
constexpr float kFovYDegrees = 60.0F;
constexpr float kNearPlane = 1.0F;
constexpr float kFarPlane = 1000.0F;

/// The corridor's cross-section is the square [-1, 1]^2, from z = -2 to -8.
constexpr float kHallwayNearZ = -2.0F;
constexpr float kHallwayFarZ = -8.0F;
constexpr float kHallwayHalfWidth = 1.0F;

constexpr glm::vec4 kNearColor{1.0F, 0.0F, 0.0F, 1.0F};
constexpr glm::vec4 kFarColor{0.0F, 1.0F, 0.0F, 1.0F};

/// Rings of vertices along the corridor. The book's meshes have three -- near,
/// middle and far -- which gives every wall two quads and the middle a ring
/// of yellow to watch move.
constexpr int kDefaultSliceCount = 3;
constexpr int kMinSliceCount = 2;
constexpr int kMaxSliceCount = 32;

/// The cross-section, going round counter-clockwise as seen from the camera:
/// top-left, bottom-left, bottom-right, top-right. Each consecutive pair is
/// a wall -- left, floor, right. The ceiling is left open, as in the book.
constexpr std::array kCorners{
    glm::vec2{-1.0F, 1.0F},
    glm::vec2{-1.0F, -1.0F},
    glm::vec2{1.0F, -1.0F},
    glm::vec2{1.0F, 1.0F},
};
constexpr std::uint32_t kRingSize = static_cast<std::uint32_t>(kCorners.size());
constexpr std::uint32_t kWallCount = kRingSize - 1;

struct Vertex {
	glm::vec3 position;
	glm::vec4 color;
};

constexpr std::array kVertexAttributes{
    glc::AttributeDesc{
        .location = kPositionLocation,
        .components = 3,
        .type = GL_FLOAT,
        .normalized = false,
        .stride = sizeof(Vertex),
        .offset = offsetof(Vertex, position),
    },
    glc::AttributeDesc{
        .location = kColorLocation,
        .components = 4,
        .type = GL_FLOAT,
        .normalized = false,
        .stride = sizeof(Vertex),
        .offset = offsetof(Vertex, color),
    },
};

/// One program per interpolation qualifier. The vertex and fragment shaders of
/// each pair differ from the next pair's by that qualifier and nothing else --
/// except the last, which has to carry two outputs instead of one.
struct InterpolationMode {
	const char* label;
	const char* vertexShader;
	const char* fragmentShader;
	const char* onRealHallway; ///< what to expect, for the panel
};

constexpr std::array kModes{
    InterpolationMode{
        .label = "Perspective-correct (smooth)",
        .vertexShader = "smooth.vert",
        .fragmentShader = "smooth.frag",
        .onRealHallway = "Color is linear along the walls, so it is bunched toward the far "
                         "end on screen the way depth is, and the diagonal seams vanish.",
    },
    InterpolationMode{
        .label = "Window-space linear (noperspective)",
        .vertexShader = "noperspective.vert",
        .fragmentShader = "noperspective.frag",
        .onRealHallway = "Color is linear across pixels, so the far colors reach toward "
                         "the camera, and each quad's two triangles disagree along the "
                         "diagonal.",
    },
    InterpolationMode{
        .label = "Corrected by hand (noperspective, divided by W)",
        .vertexShader = "by_hand.vert",
        .fragmentShader = "by_hand.frag",
        .onRealHallway = "Identical to smooth: color/W and 1/W are lerped across pixels "
                         "and divided per fragment, which is what the hardware does.",
    },
};

/// What the faux hallway looks like under any of them.
constexpr const char* kOnFauxHallway =
    "Every vertex is at the same depth, so every vertex has the same W, and the "
    "correction has nothing to work with: all three modes look like the linear one.";

/// The corridor as `sliceCount` rings of four vertices joined by quads, red
/// at the near ring shading to green at the far one.
///
/// With `flatten` set, every vertex is slid along its line of sight onto the
/// near ring's plane -- scaled by nearZ / z, which is the perspective divide
/// done ahead of time. What comes out has the same outline on screen and the
/// same colors at the same vertices, but it is a flat picture at z = nearZ.
/// The book's FauxHallway.xml is this for three rings.
[[nodiscard]] glc::Mesh buildHallway(int sliceCount, bool flatten) {
	std::vector<Vertex> vertices;
	vertices.reserve(static_cast<std::size_t>(sliceCount) * kRingSize);

	for (int slice = 0; slice < sliceCount; ++slice) {
		const float t = static_cast<float>(slice) / static_cast<float>(sliceCount - 1);
		const float z = glm::mix(kHallwayNearZ, kHallwayFarZ, t);
		const glm::vec4 color = glm::mix(kNearColor, kFarColor, t);
		const float scale = flatten ? kHallwayNearZ / z : 1.0F;

		for (const glm::vec2& corner : kCorners) {
			vertices.push_back(Vertex{
			    .position = glm::vec3{corner * kHallwayHalfWidth, z} * scale,
			    .color = color,
			});
		}
	}

	// Each wall between two rings is a quad, split along the diagonal from
	// its near-left corner. That diagonal is where window-space
	// interpolation shows a crease: the two triangles each lerp linearly,
	// but not the same way.
	//
	// Wound counter-clockwise as seen from inside. Nothing in this scene is
	// culled -- there is nothing behind anything -- so that is hygiene, not
	// necessity.
	std::vector<std::uint32_t> indices;
	indices.reserve(static_cast<std::size_t>(sliceCount - 1) * kWallCount * 6);

	for (std::uint32_t slice = 0; slice + 1 < static_cast<std::uint32_t>(sliceCount); ++slice) {
		const std::uint32_t near = slice * kRingSize;
		const std::uint32_t far = near + kRingSize;
		for (std::uint32_t wall = 0; wall < kWallCount; ++wall) {
			const std::uint32_t a = wall;
			const std::uint32_t b = wall + 1;
			indices.insert(indices.end(),
			               {near + a, near + b, far + b, near + a, far + b, far + a});
		}
	}

	return glc::Mesh::fromInterleaved(std::as_bytes(std::span{vertices}), kVertexAttributes,
	                                  static_cast<GLsizei>(vertices.size()), indices);
}

class PerspectiveInterpolation final : public glc::App {
public:
	PerspectiveInterpolation()
	    : glc::App({
	          .window = {.title = "gltut 14 -- Perspective Interpolation"},
	          .depthTest = true,
	          .cullFace = false,
	          .clearColor = {0.0F, 0.0F, 0.0F, 1.0F},
	      }) {}

protected:
	void onInit() override {
		for (std::size_t i = 0; i < kModes.size(); ++i) {
			programs_[i] = &shaders().add(glc::paths::tutorialShader(kModes[i].vertexShader),
			                              glc::paths::tutorialShader(kModes[i].fragmentShader));
		}
		rebuildHallways();

		// The aspect ratio follows the framebuffer from here on; the book
		// hard-codes 1.0 and lets a non-square window stretch the corridor.
		camera().setPerspective(kFovYDegrees, aspect(), kNearPlane, kFarPlane);
	}

	void onUpdate(float /*deltaSeconds*/) override {
		// input() reports nothing while ImGui owns the keyboard.
		if (input().keyPressed(GLFW_KEY_P)) {
			mode_ = (mode_ + 1) % kModes.size();
		}
		if (input().keyPressed(GLFW_KEY_S)) {
			useFauxHallway_ = !useFauxHallway_;
		}
		if (input().keyPressed(GLFW_KEY_W)) {
			wireframe_ = !wireframe_;
		}
	}

	void onGui() override {
		if (ImGui::Begin("Tutorial 14")) {
			ImGui::SeparatorText("Interpolation");
			for (std::size_t i = 0; i < kModes.size(); ++i) {
				if (ImGui::RadioButton(kModes[i].label, mode_ == i)) {
					mode_ = i;
				}
			}
			ImGui::TextDisabled("(P cycles)");

			ImGui::SeparatorText("Mesh");
			if (ImGui::RadioButton("Real hallway", !useFauxHallway_)) {
				useFauxHallway_ = false;
			}
			if (ImGui::RadioButton("Faux hallway, flattened onto the near plane",
			                       useFauxHallway_)) {
				useFauxHallway_ = true;
			}
			ImGui::TextDisabled("(S)");

			if (ImGui::SliderInt("Depth slices", &sliceCount_, kMinSliceCount, kMaxSliceCount)) {
				rebuildHallways();
			}
			ImGui::Checkbox("Wireframe", &wireframe_);
			ImGui::SameLine();
			ImGui::TextDisabled("(W)");

			ImGui::SeparatorText("What to look for");
			ImGui::PushTextWrapPos(0.0F);
			ImGui::TextUnformatted(useFauxHallway_ ? kOnFauxHallway : kModes[mode_].onRealHallway);
			ImGui::PopTextWrapPos();
		}
		ImGui::End();
	}

	void onRender() override {
		// The vertices are already in camera space -- there is no model
		// matrix and no view matrix, only the projection. The book stores it
		// in the program once at startup; setting it every frame costs one
		// uniform and survives both a resize and a shader reload.
		const glc::Program& program = programs_[mode_]->get();
		program.use();
		program.set("cameraToClipMatrix", camera().projection());

		// Wireframe shows what the two meshes have in common: every edge lands
		// on the same pixels. Put back to GL_FILL before ImGui draws its panel.
		if (wireframe_) {
			GLC_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
		}
		(useFauxHallway_ ? fauxHallway_ : realHallway_).render();
		if (wireframe_) {
			GLC_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
		}

		glc::Program::unuse();
	}

private:
	std::array<glc::ReloadableProgram*, kModes.size()> programs_{};
	glc::Mesh realHallway_;
	glc::Mesh fauxHallway_;

	std::size_t mode_{0};
	bool useFauxHallway_{false};
	bool wireframe_{false};
	int sliceCount_{kDefaultSliceCount};

	void rebuildHallways() {
		realHallway_ = buildHallway(sliceCount_, false);
		fauxHallway_ = buildHallway(sliceCount_, true);
	}
};

} // namespace

int main() {
	return glc::runApp<PerspectiveInterpolation>();
}
