// Tutorial 17 (Spotlight on Textures): Double Projection.
//
// The same scene twice, side by side. The left half is an ordinary camera.
// The right half uses the same camera and the same projection, and then
// rotates the result: its cameraToClipMatrix is
//
//     rotation * perspective
//
// so the order of transforms is Model -> Left Camera -> Projection -> Right
// Camera. The right view is therefore a look at clip space -- the [-1, 1] NDC
// cube, before the divide -- from the side. Everything far away is crushed
// toward z = 1, which is why turning to a top view squashes the scene into a
// thin sliver, and why moving the left camera close to an object makes that
// object fatten up on the right.
//
// Rotating *after* the projection is only legal because a rotation leaves w
// alone: its bottom row is (0, 0, 0, 1). The divide by w happens after the
// vertex shader, and dividing then rotating gives the same result as rotating
// then dividing exactly when w is unchanged. The panel prints the bottom row
// of each matrix so you can see the projection is the only one that touches
// it.
//
// Depth clamping is on for both views, as in the book. On the right it hides
// as much as it shows: the clip volume is a cube in the *rotated* space, so
// whole slabs of the scene fall outside it, and clamping flattens them onto
// the near or far plane where they draw over things in front of them. Y turns
// it off on the right so those slabs are clipped instead.
//
// The book builds this scene from an XML scene graph, with Enter to reload it.
// Here the four nodes are a table in code, and the shaders hot-reload as in
// every other chapter.
//
// Keys: left drag orbits the left camera, shift-drag pans, wheel zooms. Right
// drag rotates the right camera; Space resets it. Y toggles depth clamping on
// the right. P pauses the spinning objects. T shows the orbit point.

#include <glcore/app.hpp>
#include <glcore/camera.hpp>
#include <glcore/cycle_timer.hpp>
#include <glcore/mesh.hpp>
#include <glcore/paths.hpp>
#include <glcore/texture.hpp>
#include <glcore/uniform_buffer.hpp>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {

constexpr GLuint kProjectionBlockBinding = 0;
constexpr GLuint kLightBlockBinding = 1;
constexpr GLuint kColorTextureUnit = 0;

constexpr float kFovYDegrees = 60.0F;
constexpr float kZNear = 1.0F;
constexpr float kZFar = 1000.0F;

/// The book's left ViewPole, converted from its quaternion to yaw and pitch.
constexpr glm::vec3 kInitialTarget{0.0F};
constexpr float kInitialDistance = 25.0F;
constexpr float kInitialYawDegrees = 45.0F;
constexpr float kInitialPitchDegrees = 20.0F;
constexpr float kMinDistance = 5.0F;
constexpr float kMaxDistance = 70.0F;
constexpr float kDegreesPerPixel = 90.0F / 250.0F;

/// The two rotating objects go round once per this many seconds.
constexpr float kSpinSeconds = 10.0F;

constexpr float kLookAtScale = 0.5F;

constexpr int kNumberOfLights = 2;
constexpr std::size_t kMaxNumberOfLights = 4;

struct ProjectionBlock {
	glm::mat4 cameraToClipMatrix;
};
static_assert(sizeof(ProjectionBlock) == 64);

struct PerLight {
	glm::vec4 cameraSpaceLightPos;
	glm::vec4 lightIntensity;
};

/// std140 layout, mirrored by hand: the padding before the array is ours.
struct LightBlock {
	glm::vec4 ambientIntensity;
	float lightAttenuation;
	float maxIntensity;
	std::array<float, 2> padding;
	std::array<PerLight, kMaxNumberOfLights> lights;
};
static_assert(sizeof(LightBlock) == 160);

enum class MeshId : std::uint8_t { Cube, ShortBar, LongBar, Count };
enum class TextureId : std::uint8_t { Stone, StonePillar, WoodPillar, Count };

constexpr auto kMeshCount = static_cast<std::size_t>(MeshId::Count);
constexpr auto kTextureCount = static_cast<std::size_t>(TextureId::Count);

constexpr std::array<const char*, kMeshCount> kMeshFiles{
    "meshes/UnitCubeTex.xml",
    "meshes/ShortBar.xml",
    "meshes/LongBar.xml",
};

constexpr std::array<const char*, kTextureCount> kTextureFiles{
    "textures/seamless_rock1_small.dds",
    "textures/rough645_small.dds",
    "textures/wood4_rotate.dds",
};

/// One entry of the book's dp_scene.xml. The orientation is kept as the file
/// writes it, x y z w -- glm's quaternion constructor wants w first.
struct Node {
	const char* name;
	MeshId mesh;
	TextureId texture;
	glm::vec3 position;
	glm::vec4 orientXyzw;
	float scale;
};

constexpr std::size_t kNodeCount = 4;
constexpr std::size_t kCubeNode = 0;
constexpr std::size_t kSpinBarNode = 3;

constexpr std::array<Node, kNodeCount> kNodes{
    {
        {
            .name = "cube",
            .mesh = MeshId::Cube,
            .texture = TextureId::Stone,
            .position = {0.0F, 1.0F, 0.0F},
            .orientXyzw = {0.0F, 0.0F, 0.0F, 1.0F},
            .scale = 3.0F,
        },
        {
            .name = "rightBar",
            .mesh = MeshId::ShortBar,
            .texture = TextureId::Stone,
            .position = {13.0F, -2.0F, 0.0F},
            .orientXyzw = {0.0F, 0.0F, 0.0F, 1.0F},
            .scale = 3.0F,
        },
        {
            .name = "leaningBar",
            .mesh = MeshId::LongBar,
            .texture = TextureId::WoodPillar,
            .position = {3.0F, -7.0F, -10.0F},
            .orientXyzw = {0.64278F, 0.0F, 0.0F, 0.76604F},
            .scale = 5.0F,
        },
        {
            .name = "spinBar",
            .mesh = MeshId::LongBar,
            .texture = TextureId::StonePillar,
            .position = {-7.0F, 0.0F, 8.0F},
            .orientXyzw = {-0.148446F, 0.554035F, 0.212003F, 0.791242F},
            .scale = 4.0F,
        },
    },
};

[[nodiscard]] glm::quat fromXyzw(const glm::vec4& xyzw) {
	return glm::quat{xyzw.w, xyzw.x, xyzw.y, xyzw.z};
}

/// The right camera: a rotation and nothing else, standing in for the book's
/// second ViewPole, of which only the orientation is ever used. Horizontal
/// drag turns about Y, vertical about the already-turned X -- the same
/// convention as a view matrix.
struct NdcRotation {
	float yawDegrees{0.0F};
	float pitchDegrees{0.0F};

	void drag(const glm::vec2& pixels) {
		yawDegrees = wrap(yawDegrees + (pixels.x * kDegreesPerPixel));
		pitchDegrees = wrap(pitchDegrees + (pixels.y * kDegreesPerPixel));
	}

	[[nodiscard]] glm::mat3 matrix() const {
		const glm::mat4 pitch =
		    glm::rotate(glm::mat4{1.0F}, glm::radians(pitchDegrees), glm::vec3{1.0F, 0.0F, 0.0F});
		const glm::mat4 yaw =
		    glm::rotate(glm::mat4{1.0F}, glm::radians(yawDegrees), glm::vec3{0.0F, 1.0F, 0.0F});
		return glm::mat3{pitch * yaw};
	}

	/// Into [-180, 180), so the sliders can show it.
	[[nodiscard]] static float wrap(float degrees) {
		return degrees - (360.0F * std::floor((degrees + 180.0F) / 360.0F));
	}
};

class DoubleProjection final : public glc::App {
public:
	DoubleProjection()
	    : glc::App({
	          .window =
	              {
	                  .width = 1400,
	                  .height = 700,
	                  .title = "gltut 17 -- Double Projection",
	                  .srgbFramebuffer = true,
	              },
	          .depthTest = true,
	          .cullFace = true,
	          .frontFace = GL_CW, // gltut's meshes are wound clockwise
	          .clearColor = {0.8F, 0.8F, 0.8F, 1.0F},
	      }) {}

protected:
	void onInit() override {
		for (std::size_t i = 0; i < kMeshCount; ++i) {
			meshes_[i] = glc::Mesh::fromXmlFile(glc::paths::asset(kMeshFiles[i]));
		}
		sphereMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitSphere.xml"));

		// dp_scene.xml marks every texture srgb="true". The files are DXT1
		// (read as the RGBA flavour, with its 1-bit alpha), so sRGB here means
		// that format's sRGB twin: the same blocks, decoded through the curve
		// after decompression.
		for (std::size_t i = 0; i < kTextureCount; ++i) {
			textures_[i] =
			    glc::loadTexture2D(glc::paths::asset(kTextureFiles[i]),
				                   {.internalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT});
		}
		sampler_ = glc::makeSampler({
		    .minFilter = GL_LINEAR_MIPMAP_LINEAR,
		    .magFilter = GL_LINEAR,
		    .wrapS = GL_REPEAT,
		    .wrapT = GL_REPEAT,
		    .maxAnisotropy = glc::maxSupportedAnisotropy(),
		});

		litProgram_ = &shaders().add(glc::paths::tutorialShader("lit_texture.vert"),
		                             glc::paths::tutorialShader("lit_texture.frag"));
		unlitProgram_ = &shaders().add(glc::paths::tutorialShader("unlit.vert"),
		                               glc::paths::tutorialShader("unlit.frag"));
		configurePrograms();

		projectionBlock_.bindToPoint(kProjectionBlockBinding);
		lightBlock_.bindToPoint(kLightBlockBinding);

		orbit_.settings().minDistance = kMinDistance;
		orbit_.settings().maxDistance = kMaxDistance;
		orbit_.settings().orbitDegreesPerPixel = kDegreesPerPixel;

		GLC_CHECK(glEnable(GL_DEPTH_CLAMP));
	}

	void onUpdate(float deltaSeconds) override {
		if (shaders().reloadCount() != appliedReloads_) {
			configurePrograms();
		}

		orbit_.update(camera(), input());
		if (input().mouseDown(GLFW_MOUSE_BUTTON_RIGHT)) {
			ndcRotation_.drag(input().mouseDelta());
		}
		spinTimer_.update(deltaSeconds);

		if (input().keyPressed(GLFW_KEY_SPACE)) {
			ndcRotation_ = {};
		}
		if (input().keyPressed(GLFW_KEY_Y)) {
			depthClampRight_ = !depthClampRight_;
		}
		if (input().keyPressed(GLFW_KEY_P)) {
			spinTimer_.togglePause();
		}
		if (input().keyPressed(GLFW_KEY_T)) {
			drawLookAtPoint_ = !drawLookAtPoint_;
		}
	}

	void onGui() override {
		if (ImGui::Begin("Tutorial 17")) {
			drawRightCameraControls();
			ImGui::Separator();
			drawSceneControls();
			if (ImGui::CollapsingHeader("Matrices")) {
				drawMatrixInfo();
			}
		}
		ImGui::End();
	}

	void onRender() override {
		const glm::ivec2 framebuffer = framebufferSize();
		// Half the width each. An odd width leaves a one-pixel gap between
		// the views rather than making them different sizes.
		const glm::ivec2 half{framebuffer.x / 2, framebuffer.y};
		if (half.x <= 0 || half.y <= 0) {
			return;
		}

		const glm::mat4& worldToCamera = camera().view();
		lightBlock_.update(buildLights(worldToCamera));
		updateSpinningNodes();

		const glm::mat4 perspective = leftProjection(half);

		projectionBlock_.update(ProjectionBlock{perspective});
		glViewport(0, 0, half.x, half.y);
		drawScene(worldToCamera);
		if (drawLookAtPoint_) {
			drawLookAtPoint();
		}

		projectionBlock_.update(ProjectionBlock{rightProjection(perspective)});
		if (!depthClampRight_) {
			glDisable(GL_DEPTH_CLAMP);
		}
		glViewport(half.x + (framebuffer.x % 2), 0, half.x, half.y);
		drawScene(worldToCamera);
		glEnable(GL_DEPTH_CLAMP);

		glViewport(0, 0, framebuffer.x, framebuffer.y);
	}

private:
	glc::OrbitController orbit_{kInitialTarget, kInitialDistance, kInitialYawDegrees,
	                            kInitialPitchDegrees};
	NdcRotation ndcRotation_;
	glc::CycleTimer spinTimer_{kSpinSeconds};

	glc::ReloadableProgram* litProgram_{};
	glc::ReloadableProgram* unlitProgram_{};
	// Rewritten twice a frame, once per view.
	glc::UniformBuffer projectionBlock_{
	    glc::UniformBuffer::forType<ProjectionBlock>(GL_STREAM_DRAW),
	};
	glc::UniformBuffer lightBlock_{glc::UniformBuffer::forType<LightBlock>(GL_STREAM_DRAW)};

	std::array<glc::Mesh, kMeshCount> meshes_{};
	glc::Mesh sphereMesh_;
	std::array<glc::Texture, kTextureCount> textures_{};
	glc::Sampler sampler_;

	/// Orientations as of this frame; the table holds the rest positions.
	std::array<glm::quat, kNodeCount> orientations_{};

	bool depthClampRight_{true};
	bool drawLookAtPoint_{false};
	int appliedReloads_{0};

	void configurePrograms() {
		const glc::Program& lit = litProgram_->get();
		lit.bindUniformBlock("Projection", kProjectionBlockBinding);
		lit.bindUniformBlock("Light", kLightBlockBinding);
		lit.use();
		lit.set("diffuseColorTex", static_cast<int>(kColorTextureUnit));
		lit.set("numberOfLights", kNumberOfLights);

		unlitProgram_->get().bindUniformBlock("Projection", kProjectionBlockBinding);

		glc::Program::unuse();
		appliedReloads_ = shaders().reloadCount();
	}

	[[nodiscard]] static glm::mat4 leftProjection(const glm::ivec2& viewSize) {
		glc::MatrixStack persMatrix;
		persMatrix.Perspective(kFovYDegrees,
		                       static_cast<float>(viewSize.x) / static_cast<float>(viewSize.y),
		                       kZNear, kZFar);
		return persMatrix.Top();
	}

	/// The book's listing, transcribed. The rotation goes on the stack first,
	/// so it is applied *last*: after the projection, to clip-space vertices.
	/// mat3 drops any translation, and its bottom row is (0, 0, 0, 1), so w
	/// passes through untouched.
	[[nodiscard]] glm::mat4 rightProjection(const glm::mat4& perspective) const {
		glc::MatrixStack persMatrix;
		persMatrix.ApplyMatrix(glm::mat4{ndcRotation_.matrix()});
		persMatrix.ApplyMatrix(perspective);
		return persMatrix.Top();
	}

	[[nodiscard]] static LightBlock buildLights(const glm::mat4& worldToCamera) {
		LightBlock block{};
		block.ambientIntensity = {0.2F, 0.2F, 0.2F, 1.0F};
		block.lightAttenuation = 1.0F / (5.0F * 5.0F);
		block.maxIntensity = 3.0F;

		// A bluish-white directional light, and a strong green point light.
		block.lights[0].lightIntensity = {2.0F, 2.0F, 2.5F, 1.0F};
		block.lights[0].cameraSpaceLightPos =
		    worldToCamera * glm::normalize(glm::vec4{-0.2F, 0.5F, 0.5F, 0.0F});
		block.lights[1].lightIntensity = glm::vec4{3.5F, 6.5F, 3.0F, 1.0F} * 1.2F;
		block.lights[1].cameraSpaceLightPos = worldToCamera * glm::vec4{5.0F, 6.0F, 0.5F, 1.0F};
		return block;
	}

	/// The cube turns about world Y. The spinning bar turns about its own Z,
	/// which is why its rest orientation goes on the left of the spin.
	void updateSpinningNodes() {
		for (std::size_t i = 0; i < kNodeCount; ++i) {
			orientations_[i] = fromXyzw(kNodes[i].orientXyzw);
		}
		const float angle = glm::radians(360.0F * spinTimer_.alpha());
		orientations_[kCubeNode] = glm::angleAxis(angle, glm::vec3{0.0F, 1.0F, 0.0F});
		orientations_[kSpinBarNode] = fromXyzw(kNodes[kSpinBarNode].orientXyzw) *
		                              glm::angleAxis(angle, glm::vec3{0.0F, 0.0F, 1.0F});
	}

	void drawScene(const glm::mat4& worldToCamera) {
		const glc::Program& program = litProgram_->get();
		program.use();

		glc::MatrixStack& stack = matrices();
		stack.SetMatrix(worldToCamera);

		// Scale, then orient, then translate -- the scene graph's order.
		for (std::size_t i = 0; i < kNodeCount; ++i) {
			const Node& node = kNodes[i];
			const glc::MatrixStack::Frame frame = stack.push();
			stack.Translate(node.position);
			stack.ApplyMatrix(glm::mat4_cast(orientations_[i]));
			stack.Scale(node.scale);

			program.set("modelToCameraMatrix", stack.Top());
			program.set("normalModelToCameraMatrix", glm::inverseTranspose(glm::mat3{stack.Top()}));

			glc::bindTextureUnit(kColorTextureUnit,
			                     textures_[static_cast<std::size_t>(node.texture)], sampler_);
			meshes_[static_cast<std::size_t>(node.mesh)].render("lit-tex");
		}
	}

	/// A small sphere at the orbit target, drawn twice: grey through
	/// everything, then white where it is not hidden. Left view only.
	void drawLookAtPoint() {
		const glc::Program& program = unlitProgram_->get();
		program.use();

		glc::MatrixStack& stack = matrices();
		stack.SetIdentity();
		stack.Translate(0.0F, 0.0F, -orbit_.distance());
		stack.Scale(kLookAtScale);
		program.set("modelToCameraMatrix", stack.Top());

		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		program.set("objectColor", glm::vec4{0.25F, 0.25F, 0.25F, 1.0F});
		sphereMesh_.render("flat");
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
		program.set("objectColor", glm::vec4{1.0F, 1.0F, 1.0F, 1.0F});
		sphereMesh_.render("flat");
	}

	void drawRightCameraControls() {
		ImGui::TextUnformatted("Right camera (rotates clip space)");
		ImGui::SliderFloat("yaw", &ndcRotation_.yawDegrees, -180.0F, 180.0F, "%.1f deg");
		ImGui::SliderFloat("pitch", &ndcRotation_.pitchDegrees, -180.0F, 180.0F, "%.1f deg");

		if (ImGui::Button("Reset")) {
			ndcRotation_ = {};
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(Space)");
		ImGui::SameLine();
		// The book's Figure 17.2: look down on the NDC cube.
		if (ImGui::Button("Top view")) {
			ndcRotation_ = {
			    .yawDegrees = 0.0F,
			    .pitchDegrees = 90.0F,
			};
		}
		ImGui::SameLine();
		if (ImGui::Button("Side view")) {
			ndcRotation_ = {
			    .yawDegrees = 90.0F,
			    .pitchDegrees = 0.0F,
			};
		}

		ImGui::Checkbox("Depth clamp (right)", &depthClampRight_);
		ImGui::SameLine();
		ImGui::TextDisabled("(Y)");
	}

	void drawSceneControls() {
		bool paused = spinTimer_.isPaused();
		if (ImGui::Checkbox("Paused", &paused)) {
			spinTimer_.setPaused(paused);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(P)");

		ImGui::Checkbox("Show orbit point", &drawLookAtPoint_);
		ImGui::SameLine();
		ImGui::TextDisabled("(T)");

		ImGui::Text("left camera distance  %.1f", static_cast<double>(orbit_.distance()));
	}

	/// The bottom row of a matrix is what produces w. The projection's is
	/// (0, 0, -1, 0) -- w = -z, the perspective divide's denominator. The
	/// rotation's is (0, 0, 0, 1), so the product's bottom row is the
	/// projection's, and the divide is the same on both sides.
	void drawMatrixInfo() {
		const glm::ivec2 framebuffer = framebufferSize();
		const glm::ivec2 half{std::max(framebuffer.x / 2, 1), std::max(framebuffer.y, 1)};
		const glm::mat4 perspective = leftProjection(half);
		const glm::mat4 rotation{ndcRotation_.matrix()};

		drawBottomRow("projection", perspective);
		drawBottomRow("rotation", rotation);
		drawBottomRow("rotation * projection", rightProjection(perspective));
	}

	static void drawBottomRow(const char* label, const glm::mat4& matrix) {
		// glm is column-major: row 3 is element 3 of each column.
		ImGui::Text("%-22s [%5.2f %5.2f %5.2f %5.2f]", label, static_cast<double>(matrix[0][3]),
		            static_cast<double>(matrix[1][3]), static_cast<double>(matrix[2][3]),
		            static_cast<double>(matrix[3][3]));
	}
};

} // namespace

int main() {
	return glc::runApp<DoubleProjection>();
}
