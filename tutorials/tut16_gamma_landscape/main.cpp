// Tutorial 16 (Gamma and Textures): Gamma Landscape -- sRGB and the Screen.
//
// A textured, lit terrain under a day/night cycle, and the last piece of the
// chapter: gamma correction done by the framebuffer instead of the shader.
//
// The two earlier tutorials ended every fragment shader with pow(c, 1/2.2).
// That works, but it has to be in every shader that writes to the screen, and
// it runs before blending -- so a blend averages encoded values, the same
// mistake the badly-built mipmaps made. GL_FRAMEBUFFER_SRGB moves the encoding
// into the output stage: the shader writes linear values, blending happens in
// linear, and the hardware encodes what lands in the sRGB framebuffer. The
// glClear colour is encoded too, which is why the sky changes with it.
//
// The rest is linear end to end. terrain_tex.dds is uploaded as
// GL_SRGB8_ALPHA8, so the texture fetch decodes; lights add in linear; the
// shader divides by the scene's maximum intensity, as in Tutorial 12; and
// light_env.xml's values were tuned against an sRGB display. Space turns the
// framebuffer conversion off, and with nothing else changed the whole scene
// drops into murk -- the linear values shown as if they were encoded.
//
// The lighting is read from light_env.xml next to this file, and reloaded
// whenever it changes on disk, so the tables can be tuned live. A file that
// fails to parse is reported and the last good one stays in use.
//
// Keys: Space toggles the sRGB framebuffer. 1 and 2 pick trilinear or
// anisotropic filtering. P pauses time; - and = step it by a second. T shows
// the camera's orbit point. Left drag orbits, shift-drag pans, wheel zooms.

#include "light_env.hpp"

#include <glcore/app.hpp>
#include <glcore/camera.hpp>
#include <glcore/mesh.hpp>
#include <glcore/paths.hpp>
#include <glcore/texture.hpp>
#include <glcore/uniform_buffer.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <imgui.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

namespace {

constexpr GLuint kProjectionBlockBinding = 0;
constexpr GLuint kLightBlockBinding = 1;
constexpr GLuint kColorTextureUnit = 0;

constexpr float kFovYDegrees = 60.0F;
constexpr float kZNear = 1.0F;
constexpr float kZFar = 1000.0F;

/// The book's initial ViewPole state, converted from its quaternion to the
/// orbit controller's yaw and pitch. Its radius limits come along too.
constexpr glm::vec3 kInitialTarget{-60.257084F, 10.947238F, 62.636356F};
constexpr float kInitialDistance = 30.0F;
constexpr float kInitialYawDegrees = -24.5F;
constexpr float kInitialPitchDegrees = 11.6F;
constexpr float kMinDistance = 5.0F;
constexpr float kMaxDistance = 90.0F;
constexpr float kOrbitDegreesPerPixel = 90.0F / 250.0F;

/// The sun is drawn as a big sphere far off along its direction, the lamps as
/// unit spheres where they are.
constexpr float kSunMarkerDistance = 500.0F;
constexpr float kSunMarkerScale = 30.0F;

constexpr float kScrubSeconds = 1.0F;

/// The file's hour 0 is noon; the clock in the panel shows a wall clock.
constexpr float kClockOffsetHours = 12.0F;
constexpr float kHoursPerDay = 24.0F;

struct ProjectionBlock {
	glm::mat4 cameraToClipMatrix;
};
static_assert(sizeof(ProjectionBlock) == 64);

struct SamplerPreset {
	const char* label;
	bool anisotropic;
};

constexpr std::size_t kSamplerCount = 2;
constexpr std::array<SamplerPreset, kSamplerCount> kSamplers{
    {
        {.label = "Linear, linear mipmap", .anisotropic = false},
        {.label = "Max anisotropic", .anisotropic = true},
    },
};

[[nodiscard]] std::optional<std::filesystem::file_time_type>
lastWriteTime(const std::filesystem::path& path) {
	std::error_code error;
	const auto time = std::filesystem::last_write_time(path, error);
	if (error) {
		return std::nullopt;
	}
	return time;
}

[[nodiscard]] LightEnv loadLightEnvOrThrow(const std::filesystem::path& path) {
	auto loaded = LightEnv::tryFromXmlFile(path);
	if (!loaded) {
		throw std::runtime_error(loaded.error());
	}
	return std::move(*loaded);
}

class GammaLandscape final : public glc::App {
public:
	GammaLandscape()
	    : glc::App({
	          // Asks for a framebuffer that can encode on write. Without it,
	          // GL_FRAMEBUFFER_SRGB is allowed to do nothing at all.
	          .window = {.title = "gltut 16 -- Gamma Landscape", .srgbFramebuffer = true},
	          .depthTest = true,
	          .cullFace = true,
	          .frontFace = GL_CW, // gltut's meshes are wound clockwise
	          .autoClear = false, // cleared in onRender, after the sRGB switch
	      }),
	      lightEnvPath_{glc::paths::tutorialFile("light_env.xml")},
	      lightEnv_{loadLightEnvOrThrow(lightEnvPath_)},
	      lightEnvWriteTime_{lastWriteTime(lightEnvPath_)} {}

protected:
	void onInit() override {
		terrainMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/Terrain.xml"));
		sphereMesh_ = glc::Mesh::fromXmlFile(glc::paths::asset("meshes/UnitSphere.xml"));

		litProgram_ = &shaders().add(glc::paths::tutorialShader("lit_texture.vert"),
		                             glc::paths::tutorialShader("lit_texture.frag"));
		unlitProgram_ = &shaders().add(glc::paths::tutorialShader("unlit.vert"),
		                               glc::paths::tutorialShader("unlit.frag"));
		configurePrograms();

		projectionBlock_.bindToPoint(kProjectionBlockBinding);
		lightBlock_.bindToPoint(kLightBlockBinding);

		// The texture was painted on an sRGB monitor, so its bytes are
		// encoded. GL_SRGB8_ALPHA8 decodes them on fetch -- before filtering,
		// so the mip levels and the anisotropic taps blend linear values.
		terrainTexture_ = glc::loadTexture2D(glc::paths::asset("textures/terrain_tex.dds"),
		                                     {.internalFormat = GL_SRGB8_ALPHA8});

		const float maxAnisotropy = glc::maxSupportedAnisotropy();
		for (std::size_t i = 0; i < kSamplerCount; ++i) {
			samplers_[i] = glc::makeSampler({
			    .minFilter = GL_LINEAR_MIPMAP_LINEAR,
			    .magFilter = GL_LINEAR,
			    .wrapS = GL_REPEAT,
			    .wrapT = GL_REPEAT,
			    .maxAnisotropy = kSamplers[i].anisotropic ? maxAnisotropy : 1.0F,
			});
		}

		camera().setPerspective(kFovYDegrees, aspect(), kZNear, kZFar);
		projectionBlock_.update(ProjectionBlock{camera().projection()});

		orbit_.settings().minDistance = kMinDistance;
		orbit_.settings().maxDistance = kMaxDistance;
		orbit_.settings().orbitDegreesPerPixel = kOrbitDegreesPerPixel;

		GLC_CHECK(glEnable(GL_DEPTH_CLAMP));
	}

	void onResize(int /*width*/, int /*height*/) override {
		projectionBlock_.update(ProjectionBlock{camera().projection()});
	}

	void onUpdate(float deltaSeconds) override {
		if (shaders().reloadCount() != appliedReloads_) {
			configurePrograms();
		}
		reloadLightEnvIfChanged();

		orbit_.update(camera(), input());
		lightEnv_.update(deltaSeconds);

		if (input().keyPressed(GLFW_KEY_SPACE)) {
			srgbFramebuffer_ = !srgbFramebuffer_;
		}
		if (input().keyPressed(GLFW_KEY_P)) {
			lightEnv_.togglePause();
		}
		if (input().keyPressed(GLFW_KEY_MINUS)) {
			lightEnv_.rewind(kScrubSeconds);
		}
		if (input().keyPressed(GLFW_KEY_EQUAL)) {
			lightEnv_.fastForward(kScrubSeconds);
		}
		if (input().keyPressed(GLFW_KEY_T)) {
			drawLookAtPoint_ = !drawLookAtPoint_;
		}
		for (std::size_t i = 0; i < kSamplerCount; ++i) {
			if (input().keyPressed(GLFW_KEY_1 + static_cast<int>(i))) {
				currentSampler_ = i;
			}
		}
	}

	void onGui() override {
		if (ImGui::Begin("Tutorial 16")) {
			if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawDisplayControls();
			}
			if (ImGui::CollapsingHeader("Time", ImGuiTreeNodeFlags_DefaultOpen)) {
				drawTimeControls();
			}
			if (ImGui::CollapsingHeader("Lighting")) {
				drawLightingInfo();
			}
		}
		ImGui::End();
	}

	void onRender() override {
		// Set before the clear: the clear colour goes through the same
		// conversion as everything drawn. ImGui switches it off for its own
		// pass and back on afterwards, since its colours are encoded already.
		if (srgbFramebuffer_) {
			glEnable(GL_FRAMEBUFFER_SRGB);
		} else {
			glDisable(GL_FRAMEBUFFER_SRGB);
		}

		const glm::vec4 background = lightEnv_.backgroundColor();
		glClearColor(background.r, background.g, background.b, background.a);
		GLC_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

		lightBlock_.update(lightEnv_.toBlock(camera().view()));

		glc::MatrixStack& stack = matrices();
		stack.SetMatrix(camera().view());

		{
			const glc::MatrixStack::Frame terrainFrame = stack.push();
			stack.RotateX(-90.0F); // the terrain was modelled Z-up
			drawTerrain();
		}

		{
			const glc::MatrixStack::Frame sunFrame = stack.push();
			stack.Translate(lightEnv_.sunDirection() * kSunMarkerDistance);
			stack.Scale(kSunMarkerScale);
			drawMarker(lightEnv_.sunScaledIntensity());
		}

		for (std::size_t i = 0; i < lightEnv_.pointLightCount(); ++i) {
			const glc::MatrixStack::Frame lampFrame = stack.push();
			stack.Translate(lightEnv_.pointLightPosition(i));
			drawMarker(lightEnv_.pointLightScaledIntensity(i));
		}

		if (drawLookAtPoint_) {
			drawLookAtPoint();
		}
	}

private:
	std::filesystem::path lightEnvPath_;
	LightEnv lightEnv_;
	std::optional<std::filesystem::file_time_type> lightEnvWriteTime_;
	std::string lightEnvError_;

	glc::OrbitController orbit_{kInitialTarget, kInitialDistance, kInitialYawDegrees,
	                            kInitialPitchDegrees};

	glc::ReloadableProgram* litProgram_{};
	glc::ReloadableProgram* unlitProgram_{};
	glc::UniformBuffer projectionBlock_{glc::UniformBuffer::forType<ProjectionBlock>()};
	glc::UniformBuffer lightBlock_{glc::UniformBuffer::forType<LightBlock>(GL_STREAM_DRAW)};

	glc::Mesh terrainMesh_;
	glc::Mesh sphereMesh_;

	glc::Texture terrainTexture_;
	std::array<glc::Sampler, kSamplerCount> samplers_{};

	std::size_t currentSampler_{0};
	bool srgbFramebuffer_{true};
	bool drawLookAtPoint_{false};
	int appliedReloads_{0};

	void configurePrograms() {
		const glc::Program& lit = litProgram_->get();
		lit.bindUniformBlock("Projection", kProjectionBlockBinding);
		lit.bindUniformBlock("Light", kLightBlockBinding);
		lit.use();
		lit.set("diffuseColorTex", static_cast<int>(kColorTextureUnit));

		unlitProgram_->get().bindUniformBlock("Projection", kProjectionBlockBinding);

		glc::Program::unuse();
		appliedReloads_ = shaders().reloadCount();
	}

	/// The book has this as a commented-out key; here it happens on save.
	/// Time of day and pause carry over, so a tweak to one keyframe can be
	/// judged at the moment it was made for.
	void reloadLightEnvIfChanged() {
		const auto writeTime = lastWriteTime(lightEnvPath_);
		if (!writeTime || writeTime == lightEnvWriteTime_) {
			return;
		}
		lightEnvWriteTime_ = writeTime;

		auto loaded = LightEnv::tryFromXmlFile(lightEnvPath_);
		if (!loaded) {
			lightEnvError_ = loaded.error();
			glc::log::error("{}", lightEnvError_);
			return;
		}

		loaded->setPaused(lightEnv_.isPaused());
		loaded->fastForward(lightEnv_.sunElapsed());
		lightEnv_ = std::move(*loaded);
		lightEnvError_.clear();
		glc::log::info("reloaded {}", lightEnvPath_.filename().string());
	}

	void drawTerrain() {
		const glc::Program& program = litProgram_->get();
		program.use();

		// No normal matrix: the terrain's transform is a rotation, which
		// carries normals correctly by itself -- the book makes the same
		// assumption in its vertex shader.
		program.set("modelToCameraMatrix", matrices().Top());
		program.set("numberOfLights", static_cast<int>(lightEnv_.lightCount()));

		glc::bindTextureUnit(kColorTextureUnit, terrainTexture_, samplers_[currentSampler_]);
		terrainMesh_.render("lit-tex");
	}

	void drawMarker(const glm::vec4& color) {
		const glc::Program& program = unlitProgram_->get();
		program.use();
		program.set("modelToCameraMatrix", matrices().Top());
		program.set("objectColor", color);
		sphereMesh_.render("flat");
	}

	/// A small sphere at the orbit target, drawn twice: grey through
	/// everything, then white where it is not hidden, so it can be found
	/// behind a hill.
	void drawLookAtPoint() {
		glc::MatrixStack& stack = matrices();
		const glc::MatrixStack::Frame frame = stack.push();
		stack.SetIdentity();
		stack.Translate(0.0F, 0.0F, -orbit_.distance());

		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		drawMarker({0.25F, 0.25F, 0.25F, 1.0F});
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
		drawMarker({1.0F, 1.0F, 1.0F, 1.0F});
	}

	void drawDisplayControls() {
		ImGui::Checkbox("sRGB framebuffer", &srgbFramebuffer_);
		ImGui::SameLine();
		ImGui::TextDisabled("(Space)");

		for (std::size_t i = 0; i < kSamplerCount; ++i) {
			if (ImGui::RadioButton(kSamplers[i].label, currentSampler_ == i)) {
				currentSampler_ = i;
			}
			ImGui::SameLine();
			ImGui::TextDisabled("(%zu)", i + 1);
		}

		ImGui::Checkbox("Show orbit point", &drawLookAtPoint_);
		ImGui::SameLine();
		ImGui::TextDisabled("(T)");
	}

	void drawTimeControls() {
		const float fileHours = lightEnv_.sunAlpha() * kHoursPerDay;
		const float clockHours = std::fmod(fileHours + kClockOffsetHours, kHoursPerDay);
		const auto hour = static_cast<int>(clockHours);
		const auto minute = static_cast<int>((clockHours - static_cast<float>(hour)) * 60.0F);
		ImGui::Text("%02d:%02d", hour, minute);

		float scrubHours = clockHours;
		if (ImGui::SliderFloat("##time", &scrubHours, 0.0F, kHoursPerDay, "%05.2f h")) {
			const float file =
			    std::fmod(scrubHours - kClockOffsetHours + kHoursPerDay, kHoursPerDay);
			lightEnv_.setSunAlpha(file / kHoursPerDay);
		}

		bool paused = lightEnv_.isPaused();
		if (ImGui::Checkbox("Paused", &paused)) {
			lightEnv_.setPaused(paused);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(P)");
		ImGui::SameLine();
		if (ImGui::SmallButton("-1s")) {
			lightEnv_.rewind(kScrubSeconds);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("+1s")) {
			lightEnv_.fastForward(kScrubSeconds);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(- =)");
	}

	void drawLightingInfo() {
		ImGui::TextDisabled("%s", lightEnvPath_.filename().string().c_str());
		if (!lightEnvError_.empty()) {
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0F);
			ImGui::TextColored(ImVec4{1.0F, 0.4F, 0.4F, 1.0F}, "%s", lightEnvError_.c_str());
			ImGui::PopTextWrapPos();
		}

		ImGui::Text("max intensity  %.2f", static_cast<double>(lightEnv_.maxIntensity()));
		drawIntensityRow("ambient", lightEnv_.ambientIntensity());
		drawIntensityRow("sun", lightEnv_.sunIntensity());
		for (std::size_t i = 0; i < lightEnv_.pointLightCount(); ++i) {
			ImGui::PushID(static_cast<int>(i));
			drawIntensityRow("lamp", lightEnv_.pointLightIntensity(i));
			ImGui::PopID();
		}
	}

	/// The swatch is the intensity as the scene shows it -- divided by the
	/// maximum, then encoded the way the framebuffer would, since ImGui draws
	/// with the conversion off.
	void drawIntensityRow(const char* label, const glm::vec4& intensity) {
		const glm::vec3 scaled =
		    glm::clamp(glm::vec3{intensity} / lightEnv_.maxIntensity(), 0.0F, 1.0F);
		const float encode = srgbFramebuffer_ ? 1.0F / 2.2F : 1.0F;
		const ImVec4 swatch{std::pow(scaled.r, encode), std::pow(scaled.g, encode),
		                    std::pow(scaled.b, encode), 1.0F};
		ImGui::ColorButton(label, swatch,
		                   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoAlpha,
		                   ImVec2{ImGui::GetFrameHeight(), ImGui::GetFrameHeight()});
		ImGui::SameLine();
		ImGui::Text("%-8s  %.2f %.2f %.2f", label, static_cast<double>(intensity.r),
		            static_cast<double>(intensity.g), static_cast<double>(intensity.b));
	}
};

} // namespace

int main() {
	return glc::runApp<GammaLandscape>();
}
