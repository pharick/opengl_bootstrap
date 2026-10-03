// Tutorial 17 (Spotlight on Textures): Cube Point Light.
//
// A point light whose brightness depends on direction. The projected light
// was limited to a frustum; a point light shines everywhere, so its intensity
// needs a texture indexed by a direction rather than a 2D coordinate. That is
// what a cube map is: six square images, one per face of a cube, sampled with
// a 3D vector. The hardware picks the face from the vector's largest
// component and the texel from the other two.
//
// A direction needs a frame, so the light gains an orientation -- a point
// light never needed one, since nothing it did could show it. Its position
// and orientation define a space, the light at the origin; a scene point's
// position in that space *is* the direction from the light to the point. As
// in Projected Light the shaders get view-camera-space positions, so the
// matrix starts there:
//
//     light space <- world to light <- view camera to world
//
// No projection, no divide, no w > 0 test and no border colour this time:
// every direction lands somewhere on the cube.
//
// The axes show the light's frame, and so the cube map's: red +X, green +Y,
// blue +Z. GL_TEXTURE_CUBE_MAP_SEAMLESS is a GL 3.2 feature the book leaves
// off; the panel can turn it on, which filters across face edges instead of
// clamping at them.
//
// The book builds this scene from projCube_scene.xml, with Enter to reload it.
// Here the six nodes are a table in code, and the shaders hot-reload as in
// every other chapter.
//
// Keys: left drag orbits the camera, shift-drag pans, wheel zooms. Right drag
// turns the light, relative to the view; Space resets it. 1/2 pick the cube
// map. G toggles the other lights. P pauses the spinning objects. T shows the
// orbit point.

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

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace {

constexpr GLuint kProjectionBlockBinding = 0;
constexpr GLuint kLightBlockBinding = 1;
constexpr GLuint kColorTextureUnit = 0;
/// Bound once per frame for every object, so the scene never touches it.
constexpr GLuint kLightCubeTextureUnit = 3;

constexpr float kFovYDegrees = 60.0F;
constexpr float kZNear = 1.0F;
constexpr float kZFar = 1000.0F;

/// The book's ViewPole, converted from its quaternion to yaw and pitch.
constexpr glm::vec3 kInitialTarget{0.0F, 0.0F, 10.0F};
constexpr float kInitialDistance = 25.0F;
constexpr float kInitialYawDegrees = 45.0F;
constexpr float kInitialPitchDegrees = 20.0F;
constexpr float kMinDistance = 5.0F;
constexpr float kMaxDistance = 70.0F;
constexpr float kDegreesPerPixel = 90.0F / 250.0F;

/// The two rotating objects go round once per this many seconds.
constexpr float kSpinSeconds = 10.0F;

constexpr float kLookAtScale = 0.5F;

/// The axes mesh is half a unit long; this makes each arm 7.5.
constexpr float kAxesScale = 15.0F;

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

enum class MeshId : std::uint8_t { Cube, ShortBar, LongBar, Diorama, Floor, Count };
enum class TextureId : std::uint8_t {
	Stone,
	ConcreteWall,
	SandyGround,
	StonePillar,
	WoodPillar,
	Count,
};

constexpr auto kMeshCount = static_cast<std::size_t>(MeshId::Count);
constexpr auto kTextureCount = static_cast<std::size_t>(TextureId::Count);

constexpr std::array<const char*, kMeshCount> kMeshFiles{
    "meshes/UnitCubeTex.xml", "meshes/ShortBar.xml",  "meshes/LongBar.xml",
    "meshes/UnitDiorama.xml", "meshes/UnitPlane.xml",
};

constexpr std::array<const char*, kTextureCount> kTextureFiles{
    "textures/seamless_rock1_small.dds", "textures/concrete649_small.dds",
    "textures/dsc_1621_small.dds",       "textures/rough645_small.dds",
    "textures/wood4_rotate.dds",
};

/// What the light shines with. DXT1 cube maps, linear: the texels are light
/// intensities, not colours to be decoded.
struct LightCubeMap {
	const char* file;
	const char* name;
};

constexpr std::array<LightCubeMap, 2> kLightCubeMaps{
    {
        {.file = "textures/IrregularPoint.dds", .name = "Irregular Point Light"},
        {.file = "textures/Planetarium.dds", .name = "Planetarium"},
    },
};

/// One entry of the book's projCube_scene.xml -- the same scene as Projected
/// Light. The orientation is kept as the file writes it, x y z w -- glm's
/// quaternion constructor wants w first.
struct Node {
	const char* name;
	MeshId mesh;
	TextureId texture;
	glm::vec3 position;
	glm::vec4 orientXyzw;
	float scale;
};

constexpr std::size_t kNodeCount = 6;
constexpr std::size_t kCubeNode = 0;
constexpr std::size_t kSpinBarNode = 3;

constexpr glm::vec4 kNoRotation{0.0F, 0.0F, 0.0F, 1.0F};

constexpr std::array<Node, kNodeCount> kNodes{
    {
        {
            .name = "cube",
            .mesh = MeshId::Cube,
            .texture = TextureId::Stone,
            .position = {0.0F, 1.0F, 0.0F},
            .orientXyzw = kNoRotation,
            .scale = 3.0F,
        },
        {
            .name = "rightBar",
            .mesh = MeshId::ShortBar,
            .texture = TextureId::Stone,
            .position = {13.0F, -2.0F, 0.0F},
            .orientXyzw = kNoRotation,
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
        // The background box: three walls, open towards the camera.
        {
            .name = "diorama",
            .mesh = MeshId::Diorama,
            .texture = TextureId::ConcreteWall,
            .position = {0.0F, -10.0F, 0.0F},
            .orientXyzw = kNoRotation,
            .scale = 47.0F,
        },
        {
            .name = "floor",
            .mesh = MeshId::Floor,
            .texture = TextureId::SandyGround,
            .position = {0.0F, -33.5F, 0.0F},
            .orientXyzw = kNoRotation,
            .scale = 47.0F,
        },
    },
};

[[nodiscard]] glm::quat fromXyzw(const glm::vec4& xyzw) {
	return glm::quat{xyzw.w, xyzw.x, xyzw.y, xyzw.z};
}

/// The light as an object in the world, standing in for the book's
/// ObjectPole: a position and an orientation, giving a light-to-world matrix
/// -- the inverse of what the flashlight's camera-style pole gave.
///
/// The drag turns it about the *view's* axes, as the ObjectPole does when it
/// is given a view: horizontal drag about the screen's vertical, vertical
/// about its horizontal, whichever way the light currently faces.
struct LightObject {
	static constexpr glm::vec3 kInitialPosition{0.0F, 0.0F, 10.0F};

	glm::vec3 position{kInitialPosition};
	glm::quat orient{1.0F, 0.0F, 0.0F, 0.0F};

	[[nodiscard]] glm::mat4 lightToWorld() const {
		return glm::translate(glm::mat4{1.0F}, position) * glm::mat4_cast(orient);
	}

	void drag(const glm::vec2& pixels, const glm::mat4& worldToCamera) {
		const glm::quat cameraRotation = glm::quat_cast(glm::mat3{worldToCamera});
		const glm::quat inCameraSpace =
		    glm::angleAxis(glm::radians(pixels.x * kDegreesPerPixel), glm::vec3{0.0F, 1.0F, 0.0F}) *
		    glm::angleAxis(glm::radians(pixels.y * kDegreesPerPixel), glm::vec3{1.0F, 0.0F, 0.0F});
		// Into camera space, turn there, and back out to world space.
		orient = glm::normalize(glm::conjugate(cameraRotation) * inCameraSpace * cameraRotation *
		                        orient);
	}
};

class CubePointLight final : public glc::App {
public:
	CubePointLight()
	    : glc::App({
	          .window =
	              {
	                  .width = 1280,
	                  .height = 800,
	                  .title = "gltut 17 -- Cube Point Light",
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
		axesMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitAxes.xml"));

		// DXT1 surfaces marked srgb="true" in the scene file, as in Double
		// Projection.
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

		for (std::size_t i = 0; i < kLightCubeMaps.size(); ++i) {
			lightCubeMaps_[i] = glc::loadTextureCube(glc::paths::asset(kLightCubeMaps[i].file));
		}
		// One level, so plain linear filtering. A cube map takes three
		// coordinates, so R needs a wrap mode too; makeSampler gives it S's.
		lightSampler_ = glc::makeSampler({
		    .minFilter = GL_LINEAR,
		    .magFilter = GL_LINEAR,
		    .wrapS = GL_CLAMP_TO_EDGE,
		    .wrapT = GL_CLAMP_TO_EDGE,
		});

		cubeProgram_ = &shaders().add(glc::paths::tutorialShader("cube_light.vert"),
		                              glc::paths::tutorialShader("cube_light.frag"));
		unlitProgram_ = &shaders().add(glc::paths::tutorialShader("unlit.vert"),
		                               glc::paths::tutorialShader("unlit.frag"));
		coloredProgram_ = &shaders().add(glc::paths::tutorialShader("colored.vert"),
		                                 glc::paths::tutorialShader("colored.frag"));
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
			light_.drag(input().mouseDelta(), camera().view());
		}
		spinTimer_.update(deltaSeconds);

		if (input().keyPressed(GLFW_KEY_SPACE)) {
			light_ = {};
		}
		for (std::size_t i = 0; i < kLightCubeMaps.size(); ++i) {
			if (input().keyPressed(GLFW_KEY_1 + static_cast<int>(i))) {
				cubeMapIndex_ = static_cast<int>(i);
			}
		}
		if (input().keyPressed(GLFW_KEY_G)) {
			showOtherLights_ = !showOtherLights_;
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
			drawLightControls();
			ImGui::Separator();
			drawSceneControls();
		}
		ImGui::End();
	}

	void onRender() override {
		const glm::ivec2 framebuffer = framebufferSize();
		if (framebuffer.x <= 0 || framebuffer.y <= 0) {
			return;
		}

		if (seamless_) {
			glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		} else {
			glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		}

		const glm::mat4& worldToCamera = camera().view();
		const glm::mat4 lightToWorld = light_.lightToWorld();
		lightBlock_.update(buildLights(worldToCamera));
		updateSpinningNodes();

		glc::MatrixStack persMatrix;
		persMatrix.Perspective(
		    kFovYDegrees, static_cast<float>(framebuffer.x) / static_cast<float>(framebuffer.y),
		    kZNear, kZFar);
		projectionBlock_.update(ProjectionBlock{persMatrix.Top()});

		glc::bindTextureUnit(kLightCubeTextureUnit,
		                     lightCubeMaps_[static_cast<std::size_t>(cubeMapIndex_)], lightSampler_,
		                     GL_TEXTURE_CUBE_MAP);

		drawScene(worldToCamera, lightToWorld);
		drawLightAxes(worldToCamera, lightToWorld);
		if (drawLookAtPoint_) {
			drawLookAtPoint();
		}
	}

private:
	glc::OrbitController orbit_{kInitialTarget, kInitialDistance, kInitialYawDegrees,
	                            kInitialPitchDegrees};
	LightObject light_;
	glc::CycleTimer spinTimer_{kSpinSeconds};

	glc::ReloadableProgram* cubeProgram_{};
	glc::ReloadableProgram* unlitProgram_{};
	glc::ReloadableProgram* coloredProgram_{};
	glc::UniformBuffer projectionBlock_{
	    glc::UniformBuffer::forType<ProjectionBlock>(GL_STREAM_DRAW),
	};
	glc::UniformBuffer lightBlock_{glc::UniformBuffer::forType<LightBlock>(GL_STREAM_DRAW)};

	std::array<glc::Mesh, kMeshCount> meshes_{};
	glc::Mesh sphereMesh_;
	glc::Mesh axesMesh_;
	std::array<glc::Texture, kTextureCount> textures_{};
	glc::Sampler sampler_;
	std::array<glc::Texture, kLightCubeMaps.size()> lightCubeMaps_{};
	glc::Sampler lightSampler_;

	/// Orientations as of this frame; the table holds the rest positions.
	std::array<glm::quat, kNodeCount> orientations_{};

	int cubeMapIndex_{0};
	bool seamless_{false};
	bool showOtherLights_{true};
	bool drawLookAtPoint_{false};
	int appliedReloads_{0};

	void configurePrograms() {
		const glc::Program& cube = cubeProgram_->get();
		cube.bindUniformBlock("Projection", kProjectionBlockBinding);
		cube.bindUniformBlock("Light", kLightBlockBinding);
		cube.use();
		cube.set("diffuseColorTex", static_cast<int>(kColorTextureUnit));
		cube.set("lightCubeTex", static_cast<int>(kLightCubeTextureUnit));

		unlitProgram_->get().bindUniformBlock("Projection", kProjectionBlockBinding);
		coloredProgram_->get().bindUniformBlock("Projection", kProjectionBlockBinding);

		glc::Program::unuse();
		appliedReloads_ = shaders().reloadCount();
	}

	[[nodiscard]] static LightBlock buildLights(const glm::mat4& worldToCamera) {
		LightBlock block{};
		block.ambientIntensity = {0.2F, 0.2F, 0.2F, 1.0F};
		block.lightAttenuation = 1.0F / (30.0F * 30.0F);
		block.maxIntensity = 2.0F;

		// The same dim pair as Projected Light, so the cube light stands out.
		block.lights[0].lightIntensity = {0.2F, 0.2F, 0.2F, 1.0F};
		block.lights[0].cameraSpaceLightPos =
		    worldToCamera * glm::normalize(glm::vec4{-0.2F, 0.5F, 0.5F, 0.0F});
		block.lights[1].lightIntensity = glm::vec4{3.5F, 6.5F, 3.0F, 1.0F} * 0.5F;
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

	void drawScene(const glm::mat4& worldToCamera, const glm::mat4& lightToWorld) {
		const glc::Program& program = cubeProgram_->get();
		program.use();

		// Per frame, the same for every object: the book's uniform binders.
		// Example 17.9: view camera space back to world, then world into the
		// light's space.
		program.set("numberOfLights", showOtherLights_ ? kNumberOfLights : 0);
		program.set("cameraToLightMatrix",
		            glm::inverse(lightToWorld) * glm::inverse(worldToCamera));
		// The light sits at its space's origin.
		program.set("cameraSpaceCubeLightPos", glm::vec3{worldToCamera * lightToWorld[3]});

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

	/// The light's frame -- and so the cube map's axes -- placed with the
	/// light-to-world matrix directly. Unlike the flashlight, nothing points
	/// down -Z, so nothing is flipped.
	void drawLightAxes(const glm::mat4& worldToCamera, const glm::mat4& lightToWorld) {
		const glc::Program& program = coloredProgram_->get();
		program.use();

		glc::MatrixStack& stack = matrices();
		stack.SetMatrix(worldToCamera);
		stack.ApplyMatrix(lightToWorld);
		stack.Scale(kAxesScale);
		program.set("modelToCameraMatrix", stack.Top());

		axesMesh_.render();
	}

	/// A small sphere at the orbit target, drawn twice: grey through
	/// everything, then white where it is not hidden.
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

	void drawLightControls() {
		ImGui::TextUnformatted("Cube light");

		const char* cubeMapName = kLightCubeMaps[static_cast<std::size_t>(cubeMapIndex_)].name;
		if (ImGui::BeginCombo("cube map (1/2)", cubeMapName)) {
			for (std::size_t i = 0; i < kLightCubeMaps.size(); ++i) {
				const bool selected = std::cmp_equal(i, cubeMapIndex_);
				if (ImGui::Selectable(kLightCubeMaps[i].name, selected)) {
					cubeMapIndex_ = static_cast<int>(i);
				}
			}
			ImGui::EndCombo();
		}

		ImGui::DragFloat3("position", &light_.position.x, 0.1F);
		ImGui::Checkbox("Seamless cube map filtering", &seamless_);
		ImGui::SameLine();
		ImGui::TextDisabled("(not in the book)");

		if (ImGui::Button("Reset light")) {
			light_ = {};
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(Space; right drag turns it)");
	}

	void drawSceneControls() {
		ImGui::Checkbox("Other lights", &showOtherLights_);
		ImGui::SameLine();
		ImGui::TextDisabled("(G)");

		bool paused = spinTimer_.isPaused();
		if (ImGui::Checkbox("Paused", &paused)) {
			spinTimer_.setPaused(paused);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(P)");

		ImGui::Checkbox("Show orbit point", &drawLookAtPoint_);
		ImGui::SameLine();
		ImGui::TextDisabled("(T)");
	}
};

} // namespace

int main() {
	return glc::runApp<CubePointLight>();
}
