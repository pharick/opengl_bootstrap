#include "light_env.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

#include <tinyxml2.h>

#include <array>
#include <cmath>
#include <format>
#include <numbers>
#include <span>
#include <sstream>
#include <string_view>
#include <utility>

namespace fs = std::filesystem;

namespace {

/// The file's times are hours of a 24-hour day; the interpolators want [0, 1].
constexpr float kHoursPerDay = 24.0F;

/// gltut's default when the root has no `atten`.
constexpr float kDefaultHalfBrightnessDistance = 40.0F;

/// Reads exactly N whitespace-separated floats. Anything short or trailing is
/// an error -- a vec4 written with three numbers is a typo, not a request for
/// a default alpha.
template<std::size_t N>
[[nodiscard]] std::expected<std::array<float, N>, std::string> parseFloats(std::string_view text) {
	std::istringstream stream{std::string{text}};
	std::array<float, N> values{};
	for (float& value : values) {
		if (!(stream >> value)) {
			return std::unexpected(std::format("expected {} numbers in '{}'", N, text));
		}
	}
	if (std::string rest; stream >> rest) {
		return std::unexpected(std::format("expected {} numbers in '{}'", N, text));
	}
	return values;
}

[[nodiscard]] std::expected<glm::vec4, std::string> vec4Attribute(const tinyxml2::XMLElement& node,
                                                                  const char* name) {
	const char* text = node.Attribute(name);
	if (text == nullptr) {
		return std::unexpected(std::format("<{}> has no '{}'", node.Name(), name));
	}
	return parseFloats<4>(text).transform(
	    [](const std::array<float, 4>& v) { return glm::vec4{v[0], v[1], v[2], v[3]}; });
}

[[nodiscard]] std::expected<float, std::string> floatAttribute(const tinyxml2::XMLElement& node,
                                                               const char* name) {
	float value = 0.0F;
	if (node.QueryFloatAttribute(name, &value) != tinyxml2::XML_SUCCESS) {
		return std::unexpected(std::format("<{}> has no numeric '{}'", node.Name(), name));
	}
	return value;
}

/// A clock's duration. CycleTimer rejects anything not positive by throwing;
/// this says so with the element's name instead.
[[nodiscard]] std::expected<float, std::string>
durationAttribute(const tinyxml2::XMLElement& node) {
	return floatAttribute(node, "time")
	    .and_then([&node](float seconds) -> std::expected<float, std::string> {
		    if (seconds <= 0.0F) {
			    return std::unexpected(std::format("<{}> time must be positive", node.Name()));
		    }
		    return seconds;
	    });
}

/// One row of the sun's table, every column read before any is kept, so a
/// bad row cannot leave the four tables out of step.
struct SunKey {
	float time;
	glm::vec4 ambient;
	glm::vec4 intensity;
	glm::vec4 background;
	float maxIntensity;
};

[[nodiscard]] std::expected<SunKey, std::string> parseSunKey(const tinyxml2::XMLElement& node) {
	const auto time = floatAttribute(node, "time");
	if (!time) {
		return std::unexpected(time.error());
	}
	const auto ambient = vec4Attribute(node, "ambient");
	if (!ambient) {
		return std::unexpected(ambient.error());
	}
	const auto intensity = vec4Attribute(node, "intensity");
	if (!intensity) {
		return std::unexpected(intensity.error());
	}
	const auto background = vec4Attribute(node, "background");
	if (!background) {
		return std::unexpected(background.error());
	}
	const auto maxIntensity = floatAttribute(node, "max-intensity");
	if (!maxIntensity) {
		return std::unexpected(maxIntensity.error());
	}

	if (*time < 0.0F || *time > kHoursPerDay) {
		return std::unexpected(std::format("sun key at {} h is outside the day", *time));
	}
	if (*maxIntensity <= 0.0F) {
		return std::unexpected("max-intensity must be positive -- the shader divides by it");
	}

	return SunKey{
	    .time = *time / kHoursPerDay,
	    .ambient = *ambient,
	    .intensity = *intensity,
	    .background = *background,
	    .maxIntensity = *maxIntensity,
	};
}

/// One <light>, read but not yet built: CycleTimer needs its duration up
/// front, so the lamp itself is assembled once everything has parsed.
struct LampData {
	float seconds;
	glm::vec4 intensity;
	std::vector<glm::vec3> waypoints;
};

[[nodiscard]] std::expected<LampData, std::string> parseLamp(const tinyxml2::XMLElement& node) {
	const auto onLine = [](const tinyxml2::XMLElement& element, std::string_view message) {
		return std::unexpected(std::format("line {}: {}", element.GetLineNum(), message));
	};

	const auto seconds = durationAttribute(node);
	if (!seconds) {
		return onLine(node, seconds.error());
	}
	const auto intensity = vec4Attribute(node, "intensity");
	if (!intensity) {
		return onLine(node, intensity.error());
	}

	LampData lamp{.seconds = *seconds, .intensity = *intensity, .waypoints = {}};
	for (const tinyxml2::XMLElement* key = node.FirstChildElement("key"); key != nullptr;
	     key = key->NextSiblingElement("key")) {
		const char* text = key->GetText();
		const auto xyz = parseFloats<3>(text != nullptr ? text : "");
		if (!xyz) {
			return onLine(*key, xyz.error());
		}
		lamp.waypoints.emplace_back((*xyz)[0], (*xyz)[1], (*xyz)[2]);
	}
	if (lamp.waypoints.empty()) {
		return onLine(node, "<light> needs at least one <key>");
	}
	return lamp;
}

} // namespace

std::expected<LightEnv, std::string> LightEnv::tryFromXmlFile(const fs::path& path) {
	const auto fail = [&path](std::string_view message) {
		return std::unexpected(std::format("{}: {}", path.string(), message));
	};

	tinyxml2::XMLDocument document;
	if (document.LoadFile(path.string().c_str()) != tinyxml2::XML_SUCCESS) {
		return fail(document.ErrorStr());
	}

	const tinyxml2::XMLElement* root = document.FirstChildElement("lightenv");
	if (root == nullptr) {
		return fail("no <lightenv> root element");
	}
	const tinyxml2::XMLElement* sun = root->FirstChildElement("sun");
	if (sun == nullptr) {
		return fail("<lightenv> has no <sun>");
	}

	const auto sunSeconds = durationAttribute(*sun);
	if (!sunSeconds) {
		return fail(sunSeconds.error());
	}
	LightEnv env{*sunSeconds};

	// Authored as the distance at which a lamp is at half brightness, which is
	// something a person can picture; the shader wants the coefficient.
	const float halfDistance = root->FloatAttribute("atten", kDefaultHalfBrightnessDistance);
	if (halfDistance <= 0.0F) {
		return fail("atten must be positive");
	}
	env.lightAttenuation_ = 1.0F / (halfDistance * halfDistance);

	std::vector<glc::Keyframe<glm::vec4>> ambient;
	std::vector<glc::Keyframe<glm::vec4>> intensity;
	std::vector<glc::Keyframe<glm::vec4>> background;
	std::vector<glc::Keyframe<float>> maxIntensity;

	for (const tinyxml2::XMLElement* node = sun->FirstChildElement("key"); node != nullptr;
	     node = node->NextSiblingElement("key")) {
		const auto key = parseSunKey(*node);
		if (!key) {
			return fail(std::format("line {}: {}", node->GetLineNum(), key.error()));
		}
		ambient.push_back({.time = key->time, .value = key->ambient});
		intensity.push_back({.time = key->time, .value = key->intensity});
		background.push_back({.time = key->time, .value = key->background});
		maxIntensity.push_back({.time = key->time, .value = key->maxIntensity});
	}
	if (ambient.empty()) {
		return fail("<sun> needs at least one <key>");
	}

	env.ambientInterpolator_.setKeyframes(std::span{std::as_const(ambient)});
	env.sunIntensityInterpolator_.setKeyframes(std::span{std::as_const(intensity)});
	env.backgroundInterpolator_.setKeyframes(std::span{std::as_const(background)});
	env.maxIntensityInterpolator_.setKeyframes(std::span{std::as_const(maxIntensity)});

	for (const tinyxml2::XMLElement* node = root->FirstChildElement("light"); node != nullptr;
	     node = node->NextSiblingElement("light")) {
		if (env.lightCount() == kMaxNumberOfLights) {
			return fail(std::format("line {}: more than {} <light>s -- the shader has room "
			                        "for the sun and {}",
			                        node->GetLineNum(), kMaxNumberOfLights - 1,
			                        kMaxNumberOfLights - 1));
		}

		auto parsed = parseLamp(*node);
		if (!parsed) {
			return fail(parsed.error());
		}

		Lamp lamp{
		    .timer = glc::CycleTimer{parsed->seconds},
		    .intensity = parsed->intensity,
		    .path = {},
		};
		lamp.path.setWaypoints(std::span{std::as_const(parsed->waypoints)});
		env.lamps_.push_back(std::move(lamp));
	}

	return env;
}

void LightEnv::update(float deltaSeconds) {
	sunTimer_.update(deltaSeconds);
	for (Lamp& lamp : lamps_) {
		lamp.timer.update(deltaSeconds);
	}
}

void LightEnv::setPaused(bool paused) {
	sunTimer_.setPaused(paused);
	for (Lamp& lamp : lamps_) {
		lamp.timer.setPaused(paused);
	}
}

void LightEnv::togglePause() {
	setPaused(!sunTimer_.isPaused());
}

bool LightEnv::isPaused() const {
	return sunTimer_.isPaused();
}

void LightEnv::rewind(float seconds) {
	sunTimer_.rewind(seconds);
	for (Lamp& lamp : lamps_) {
		lamp.timer.rewind(seconds);
	}
}

void LightEnv::fastForward(float seconds) {
	sunTimer_.fastForward(seconds);
	for (Lamp& lamp : lamps_) {
		lamp.timer.fastForward(seconds);
	}
}

float LightEnv::sunElapsed() const {
	return sunTimer_.elapsed();
}

float LightEnv::sunAlpha() const {
	return sunTimer_.alpha();
}

void LightEnv::setSunAlpha(float alpha) {
	sunTimer_.setAlpha(alpha);
}

glm::vec4 LightEnv::backgroundColor() const {
	return backgroundInterpolator_.interpolate(sunTimer_.alpha());
}

glm::vec4 LightEnv::ambientIntensity() const {
	return ambientInterpolator_.interpolate(sunTimer_.alpha());
}

float LightEnv::maxIntensity() const {
	return maxIntensityInterpolator_.interpolate(sunTimer_.alpha());
}

glm::vec3 LightEnv::sunDirection() const {
	const float angle = 2.0F * std::numbers::pi_v<float> * sunTimer_.alpha();
	const glm::vec4 overhead{std::sin(angle), std::cos(angle), 0.0F, 0.0F};

	// Tilted so the sun never passes exactly overhead. gltut writes
	// `glm::rotate(mat4(1), 5.0f, ...)`: glm took degrees then, radians now.
	const glm::mat4 tilt =
	    glm::rotate(glm::mat4{1.0F}, glm::radians(5.0F), glm::vec3{0.0F, 1.0F, 0.0F});
	return glm::vec3{tilt * overhead};
}

glm::vec4 LightEnv::sunIntensity() const {
	return sunIntensityInterpolator_.interpolate(sunTimer_.alpha());
}

glm::vec4 LightEnv::sunScaledIntensity() const {
	return sunIntensity() / maxIntensity();
}

glm::vec3 LightEnv::pointLightPosition(std::size_t index) const {
	const Lamp& lamp = lamps_.at(index);
	return lamp.path.interpolate(lamp.timer.alpha());
}

glm::vec4 LightEnv::pointLightIntensity(std::size_t index) const {
	return lamps_.at(index).intensity;
}

glm::vec4 LightEnv::pointLightScaledIntensity(std::size_t index) const {
	return pointLightIntensity(index) / maxIntensity();
}

LightBlock LightEnv::toBlock(const glm::mat4& worldToCamera) const {
	LightBlock block{};
	block.ambientIntensity = ambientIntensity();
	block.lightAttenuation = lightAttenuation_;
	block.maxIntensity = maxIntensity();

	// w = 0 drops the view matrix's translation, which is what a direction
	// needs; w = 1 keeps it, which is what a position needs.
	block.lights[0] = {
	    .cameraSpaceLightPos = worldToCamera * glm::vec4{sunDirection(), 0.0F},
	    .lightIntensity = sunIntensity(),
	};
	for (std::size_t i = 0; i < lamps_.size(); ++i) {
		block.lights[i + 1] = {
		    .cameraSpaceLightPos = worldToCamera * glm::vec4{pointLightPosition(i), 1.0F},
		    .lightIntensity = pointLightIntensity(i),
		};
	}
	return block;
}
