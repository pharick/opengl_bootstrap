#pragma once

#include <glcore/cycle_timer.hpp>
#include <glcore/interpolator.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

/// One light as the shader sees it. `cameraSpaceLightPos.w` selects the kind:
/// 0 makes the xyz a direction (the sun), 1 a position (a lamp).
struct PerLight {
	glm::vec4 cameraSpaceLightPos;
	glm::vec4 lightIntensity;
};

/// The sun plus up to three point lights. The shader's array is this size and
/// a `numberOfLights` uniform says how many entries are live.
constexpr std::size_t kMaxNumberOfLights = 4;

/// std140 layout, mirrored by hand: glm's vec4 is 4-byte aligned, not 16, so
/// the padding before the array is ours to write.
struct LightBlock {
	glm::vec4 ambientIntensity;
	float lightAttenuation;
	float maxIntensity;
	std::array<float, 2> padding; ///< lights[0] starts at offset 32
	std::array<PerLight, kMaxNumberOfLights> lights;
};
static_assert(sizeof(LightBlock) == 160);

/// The landscape's lighting: a day/night cycle and a few wandering lamps, all
/// read from an XML file rather than written in code.
///
/// This is gltut's LightEnv. Tutorial 12's LightManager had the same parts
/// with its tables compiled in; this chapter moved them into LightEnv.xml so
/// they could be tuned for an sRGB framebuffer without rebuilding -- which is
/// why the tutorial here reloads the file whenever it changes.
///
///     <lightenv atten="60">                   distance at which a lamp halves
///       <sun time="48">                       seconds per day
///         <key time="6" ambient="r g b a" intensity="r g b a"
///              background="r g b a" max-intensity="10"/>     time in hours
///       </sun>
///       <light time="24" intensity="r g b a"> seconds per circuit
///         <key>x y z</key> ...                world-space waypoints
///       </light>
///     </lightenv>
class LightEnv {
public:
	/// Parses the file. The error names the file and the offending element.
	[[nodiscard]] static std::expected<LightEnv, std::string>
	tryFromXmlFile(const std::filesystem::path& path);

	/// Advances the sun and every lamp. Deltas come from App::onUpdate.
	void update(float deltaSeconds);

	void setPaused(bool paused);
	void togglePause();
	[[nodiscard]] bool isPaused() const;

	/// Scrubbing, applied to every clock. Works while paused.
	void rewind(float seconds);
	void fastForward(float seconds);

	/// Seconds into the current day. With rewind/fastForward this is enough
	/// to carry the time of day across a reload.
	[[nodiscard]] float sunElapsed() const;

	/// Progress through the day, [0, 1). Hour 0 of the file -- alpha 0 -- is
	/// *noon*: the sun's direction there points straight up.
	[[nodiscard]] float sunAlpha() const;
	/// Moves the sun alone; the lamps keep their own loops.
	void setSunAlpha(float alpha);

	[[nodiscard]] glm::vec4 backgroundColor() const;
	[[nodiscard]] glm::vec4 ambientIntensity() const;

	/// The brightest value the scene can show right now; the shader divides
	/// every result by it.
	[[nodiscard]] float maxIntensity() const;

	/// Unit vector pointing *toward* the sun.
	[[nodiscard]] glm::vec3 sunDirection() const;
	[[nodiscard]] glm::vec4 sunIntensity() const;
	/// Sun intensity over maxIntensity -- the colour a marker for it should be,
	/// since that is what the same light reads as on a surface.
	[[nodiscard]] glm::vec4 sunScaledIntensity() const;

	[[nodiscard]] std::size_t pointLightCount() const noexcept {
		return lamps_.size();
	}
	/// The sun and the lamps: what the shader's `numberOfLights` wants.
	[[nodiscard]] std::size_t lightCount() const noexcept {
		return 1 + lamps_.size();
	}

	[[nodiscard]] glm::vec3 pointLightPosition(std::size_t index) const;
	[[nodiscard]] glm::vec4 pointLightIntensity(std::size_t index) const;
	[[nodiscard]] glm::vec4 pointLightScaledIntensity(std::size_t index) const;

	/// Converts the world-space lights into the camera space the shader works
	/// in. Lights past lightCount() are left zeroed.
	[[nodiscard]] LightBlock toBlock(const glm::mat4& worldToCamera) const;

private:
	struct Lamp {
		glc::CycleTimer timer;
		glm::vec4 intensity;
		glc::ConstVelLinearInterpolator<glm::vec3> path;
	};

	explicit LightEnv(float sunSeconds) : sunTimer_{sunSeconds} {}

	/// k in `I / (1 + k * d*d)`, from the file's `atten`.
	float lightAttenuation_{};

	glc::CycleTimer sunTimer_;
	glc::TimedLinearInterpolator<glm::vec4> ambientInterpolator_;
	glc::TimedLinearInterpolator<glm::vec4> sunIntensityInterpolator_;
	glc::TimedLinearInterpolator<glm::vec4> backgroundInterpolator_;
	glc::TimedLinearInterpolator<float> maxIntensityInterpolator_;

	std::vector<Lamp> lamps_;
};
