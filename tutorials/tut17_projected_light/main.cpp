// Tutorial 17 (Spotlight on Textures): Projected Light.
//
// A flashlight made of a texture. To make a texture look projected onto the
// scene, think backwards: project the scene onto the texture. Every vertex
// goes through a second camera -- the flashlight's -- and a second
// perspective projection, and where it lands in that projection is where it
// reads the texture. The texel is then used as the intensity of an ordinary
// point light sitting at the flashlight's position.
//
// The scene graph hands the shaders positions in the *viewing* camera's
// space, so the matrix the shaders get starts from there:
//
//     texture space <- [-1, 1] to [0, 1] <- light projection
//                   <- world to light camera <- view camera to world
//
// The [-1, 1] to [0, 1] step is a translate and scale applied after the
// projection, before the divide -- the same kind of post-projective transform
// as Double Projection's rotation, and legal for the same reason: it leaves w
// alone. The divide itself is textureProj's job, in the fragment shader.
//
// Two things the projection does not do for us. It does not clip, so points
// behind the flashlight project too (upside down); the shader rejects w <= 0.
// And it does not stop at the texture's edge: with GL_CLAMP_TO_EDGE a
// non-black edge texel (the third texture) floods everything outside the cone.
// GL_CLAMP_TO_BORDER with a black border fixes that; H switches between them.
//
// The book builds this scene from proj2d_scene.xml, with Enter to reload it.
// Here the six nodes are a table in code, and the shaders hot-reload as in
// every other chapter.
//
// Keys: left drag orbits the camera, shift-drag pans, wheel zooms. The
// flashlight -- the red, green and blue axes; blue is where it points -- moves
// with I/K (forward, back), J/L (left, right), O/U (up, down), relative to its
// own facing; hold shift to go slowly. Right drag turns it; Space resets it.
// Y/N widen and narrow its field of view. 1/2/3 pick the light texture. H
// toggles edge vs border clamping. G toggles the other lights. P pauses the
// spinning objects. T shows the orbit point.

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
#include <cstddef>
#include <cstdint>
#include <utility>

namespace {

constexpr GLuint kProjectionBlockBinding = 0;
constexpr GLuint kLightBlockBinding = 1;
constexpr GLuint kColorTextureUnit = 0;
/// Bound once per frame for every object, so the scene never touches it.
constexpr GLuint kLightProjTextureUnit = 3;

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

/// The light's field of view steps through these, as in the book.
constexpr std::array<float, 8> kLightFovs{
    10.0F, 20.0F, 45.0F, 75.0F, 90.0F, 120.0F, 150.0F, 170.0F,
};
constexpr int kInitialLightFovIndex = 3;
constexpr auto kLastLightFovIndex = static_cast<int>(kLightFovs.size()) - 1;

/// The light's projection never clips and its depth is thrown away, so these
/// only need to make a valid perspective matrix.
constexpr float kLightZNear = 1.0F;
constexpr float kLightZFar = 100.0F;

/// The book moves the flashlight 4 units per key repeat, or 1 with shift.
/// Held keys here move it continuously instead, at about the same pace.
constexpr float kLightUnitsPerSecond = 16.0F;
constexpr float kLightSlowUnitsPerSecond = 4.0F;

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

/// What the flashlight projects. Uncompressed and linear: the texels are
/// light intensities, not colours to be decoded.
///
/// The book's files are 32-bit RGB with an unused pad byte and no alpha mask,
/// a layout gli asserts on. The copies in assets/ declare the pad byte as alpha
/// and fill it with 255; the RGB texels are the book's, byte for byte.
struct LightTexture {
	const char* file;
	const char* name;
};

constexpr std::array<LightTexture, 3> kLightTextures{
    {
        {.file = "textures/Flashlight.dds", .name = "Flashlight"},
        {.file = "textures/PointsOfLight.dds", .name = "Multiple Point Lights"},
        {.file = "textures/Bands.dds", .name = "Light Bands"},
    },
};

/// One entry of the book's proj2d_scene.xml. The orientation is kept as the
/// file writes it, x y z w -- glm's quaternion constructor wants w first.
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

/// The flashlight's camera, standing in for the book's second ViewPole: an
/// orientation about a target point, seen from `radius` behind it. The
/// flashlight sits at the eye and looks at the target, so turning it swings
/// it around a point in front of it, as in the book.
///
/// Horizontal drag turns about Y, vertical about the already-turned X -- the
/// ViewPole's convention. Movement is in the light's own frame.
struct LightPole {
	static constexpr glm::vec3 kInitialTarget{0.0F, 0.0F, 20.0F};
	static constexpr float kRadius = 5.0F;

	glm::vec3 target{kInitialTarget};
	glm::quat orient{1.0F, 0.0F, 0.0F, 0.0F};

	/// World to light camera.
	[[nodiscard]] glm::mat4 matrix() const {
		const glm::mat4 back = glm::translate(glm::mat4{1.0F}, glm::vec3{0.0F, 0.0F, -kRadius});
		return back * glm::mat4_cast(orient) * glm::translate(glm::mat4{1.0F}, -target);
	}

	void drag(const glm::vec2& pixels) {
		const glm::quat yaw =
		    glm::angleAxis(glm::radians(pixels.x * kDegreesPerPixel), glm::vec3{0.0F, 1.0F, 0.0F});
		const glm::quat pitch =
		    glm::angleAxis(glm::radians(pixels.y * kDegreesPerPixel), glm::vec3{1.0F, 0.0F, 0.0F});
		orient = glm::normalize(pitch * orient * yaw);
	}

	/// `lightOffset` is in the light's camera space: -Z is forward.
	void move(const glm::vec3& lightOffset) {
		target += glm::conjugate(orient) * lightOffset;
	}
};

class ProjectedLight final : public glc::App {
public:
	ProjectedLight()
	    : glc::App({
	          .window =
	              {
	                  .width = 1280,
	                  .height = 800,
	                  .title = "gltut 17 -- Projected Light",
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

		for (std::size_t i = 0; i < kLightTextures.size(); ++i) {
			lightTextures_[i] = glc::loadTexture2D(glc::paths::asset(kLightTextures[i].file),
			                                       {.generateMipmaps = false});
		}
		lightSamplers_[kEdgeSampler] = glc::makeSampler({
		    .minFilter = GL_LINEAR,
		    .magFilter = GL_LINEAR,
		    .wrapS = GL_CLAMP_TO_EDGE,
		    .wrapT = GL_CLAMP_TO_EDGE,
		});
		lightSamplers_[kBorderSampler] = glc::makeSampler({
		    .minFilter = GL_LINEAR,
		    .magFilter = GL_LINEAR,
		    .wrapS = GL_CLAMP_TO_BORDER,
		    .wrapT = GL_CLAMP_TO_BORDER,
		    .borderColor = {0.0F, 0.0F, 0.0F, 1.0F},
		});

		projProgram_ = &shaders().add(glc::paths::tutorialShader("proj_light.vert"),
		                              glc::paths::tutorialShader("proj_light.frag"));
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
			lightPole_.drag(input().mouseDelta());
		}
		moveLight(deltaSeconds);
		spinTimer_.update(deltaSeconds);

		if (input().keyPressed(GLFW_KEY_SPACE)) {
			lightPole_ = {};
		}
		if (input().keyPressed(GLFW_KEY_Y)) {
			lightFovIndex_ = std::min(lightFovIndex_ + 1, kLastLightFovIndex);
		}
		if (input().keyPressed(GLFW_KEY_N)) {
			lightFovIndex_ = std::max(lightFovIndex_ - 1, 0);
		}
		for (std::size_t i = 0; i < kLightTextures.size(); ++i) {
			if (input().keyPressed(GLFW_KEY_1 + static_cast<int>(i))) {
				lightTextureIndex_ = static_cast<int>(i);
			}
		}
		if (input().keyPressed(GLFW_KEY_H)) {
			borderClamp_ = !borderClamp_;
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
			drawFlashlightControls();
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

		const glm::mat4& worldToCamera = camera().view();
		const glm::mat4 worldToLight = lightPole_.matrix();
		lightBlock_.update(buildLights(worldToCamera));
		updateSpinningNodes();

		glc::MatrixStack persMatrix;
		persMatrix.Perspective(
		    kFovYDegrees, static_cast<float>(framebuffer.x) / static_cast<float>(framebuffer.y),
		    kZNear, kZFar);
		projectionBlock_.update(ProjectionBlock{persMatrix.Top()});

		glc::bindTextureUnit(kLightProjTextureUnit,
		                     lightTextures_[static_cast<std::size_t>(lightTextureIndex_)],
		                     lightSamplers_[borderClamp_ ? kBorderSampler : kEdgeSampler]);

		drawScene(worldToCamera, worldToLight);
		drawLightAxes(worldToCamera, worldToLight);
		if (drawLookAtPoint_) {
			drawLookAtPoint();
		}
	}

private:
	static constexpr std::size_t kEdgeSampler = 0;
	static constexpr std::size_t kBorderSampler = 1;

	glc::OrbitController orbit_{kInitialTarget, kInitialDistance, kInitialYawDegrees,
	                            kInitialPitchDegrees};
	LightPole lightPole_;
	glc::CycleTimer spinTimer_{kSpinSeconds};

	glc::ReloadableProgram* projProgram_{};
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
	std::array<glc::Texture, kLightTextures.size()> lightTextures_{};
	std::array<glc::Sampler, 2> lightSamplers_{};

	/// Orientations as of this frame; the table holds the rest positions.
	std::array<glm::quat, kNodeCount> orientations_{};

	int lightFovIndex_{kInitialLightFovIndex};
	int lightTextureIndex_{0};
	bool borderClamp_{false};
	bool showOtherLights_{true};
	bool drawLookAtPoint_{false};
	int appliedReloads_{0};

	void configurePrograms() {
		const glc::Program& proj = projProgram_->get();
		proj.bindUniformBlock("Projection", kProjectionBlockBinding);
		proj.bindUniformBlock("Light", kLightBlockBinding);
		proj.use();
		proj.set("diffuseColorTex", static_cast<int>(kColorTextureUnit));
		proj.set("lightProjTex", static_cast<int>(kLightProjTextureUnit));

		unlitProgram_->get().bindUniformBlock("Projection", kProjectionBlockBinding);
		coloredProgram_->get().bindUniformBlock("Projection", kProjectionBlockBinding);

		glc::Program::unuse();
		appliedReloads_ = shaders().reloadCount();
	}

	void moveLight(float deltaSeconds) {
		const bool slow =
		    input().keyDown(GLFW_KEY_LEFT_SHIFT) || input().keyDown(GLFW_KEY_RIGHT_SHIFT);
		const float step = (slow ? kLightSlowUnitsPerSecond : kLightUnitsPerSecond) * deltaSeconds;

		glm::vec3 offset{0.0F};
		const auto axis = [this](int positiveKey, int negativeKey) {
			return (input().keyDown(positiveKey) ? 1.0F : 0.0F) -
			       (input().keyDown(negativeKey) ? 1.0F : 0.0F);
		};
		offset.x = axis(GLFW_KEY_L, GLFW_KEY_J);
		offset.y = axis(GLFW_KEY_O, GLFW_KEY_U);
		offset.z = axis(GLFW_KEY_K, GLFW_KEY_I);
		lightPole_.move(offset * step);
	}

	/// Example 17.6, read bottom to top: view camera space back to world,
	/// world to the light's camera, the light's projection (square, since the
	/// textures are), and then [-1, 1] to [0, 1] in X and Y. Z passes through;
	/// nobody reads it.
	[[nodiscard]] glm::mat4 cameraToLightProjection(const glm::mat4& worldToCamera,
	                                                const glm::mat4& worldToLight) const {
		glc::MatrixStack lightProjStack;
		lightProjStack.Translate(0.5F, 0.5F, 0.0F);
		lightProjStack.Scale(0.5F, 0.5F, 1.0F);
		lightProjStack.Perspective(kLightFovs[static_cast<std::size_t>(lightFovIndex_)], 1.0F,
		                           kLightZNear, kLightZFar);
		lightProjStack.ApplyMatrix(worldToLight);
		lightProjStack.ApplyMatrix(glm::inverse(worldToCamera));
		return lightProjStack.Top();
	}

	[[nodiscard]] static LightBlock buildLights(const glm::mat4& worldToCamera) {
		LightBlock block{};
		block.ambientIntensity = {0.2F, 0.2F, 0.2F, 1.0F};
		block.lightAttenuation = 1.0F / (30.0F * 30.0F);
		block.maxIntensity = 2.0F;

		// Dimmer than in Double Projection, so the flashlight stands out.
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

	void drawScene(const glm::mat4& worldToCamera, const glm::mat4& worldToLight) {
		const glc::Program& program = projProgram_->get();
		program.use();

		// Per frame, the same for every object: the book's uniform binders.
		program.set("numberOfLights", showOtherLights_ ? kNumberOfLights : 0);
		program.set("cameraToLightProjMatrix",
		            cameraToLightProjection(worldToCamera, worldToLight));
		// The flashlight sits at the light camera's origin.
		const glm::vec4 worldLightPos = glm::inverse(worldToLight)[3];
		program.set("cameraSpaceProjLightPos", glm::vec3{worldToCamera * worldLightPos});

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

	/// The flashlight's frame, placed with the inverse of its view matrix.
	/// A camera looks down -Z, so Z is flipped to make the blue arm point
	/// where the light goes.
	void drawLightAxes(const glm::mat4& worldToCamera, const glm::mat4& worldToLight) {
		const glc::Program& program = coloredProgram_->get();
		program.use();

		glc::MatrixStack& stack = matrices();
		stack.SetMatrix(worldToCamera);
		stack.ApplyMatrix(glm::inverse(worldToLight));
		stack.Scale(kAxesScale);
		stack.Scale(1.0F, 1.0F, -1.0F);
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

	void drawFlashlightControls() {
		ImGui::TextUnformatted("Flashlight");

		const char* textureName = kLightTextures[static_cast<std::size_t>(lightTextureIndex_)].name;
		if (ImGui::BeginCombo("texture (1/2/3)", textureName)) {
			for (std::size_t i = 0; i < kLightTextures.size(); ++i) {
				const bool selected = std::cmp_equal(i, lightTextureIndex_);
				if (ImGui::Selectable(kLightTextures[i].name, selected)) {
					lightTextureIndex_ = static_cast<int>(i);
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SliderInt("fov (Y/N)", &lightFovIndex_, 0, kLastLightFovIndex, "");
		ImGui::SameLine();
		ImGui::Text("%.0f deg",
		            static_cast<double>(kLightFovs[static_cast<std::size_t>(lightFovIndex_)]));

		ImGui::Checkbox("Clamp to border", &borderClamp_);
		ImGui::SameLine();
		ImGui::TextDisabled("(H; off = clamp to edge)");

		const glm::vec3& target = lightPole_.target;
		ImGui::Text("aims at  (%.1f, %.1f, %.1f)", static_cast<double>(target.x),
		            static_cast<double>(target.y), static_cast<double>(target.z));
		if (ImGui::Button("Reset flashlight")) {
			lightPole_ = {};
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(Space)");
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
	return glc::runApp<ProjectedLight>();
}
