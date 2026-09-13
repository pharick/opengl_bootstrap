#include "lights.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <numbers>
#include <type_traits>

namespace {

constexpr glm::vec4 kSkyDaylightColor{0.65F, 0.65F, 1.0F, 1.0F};

/// One moment in the day. Authoring ambient, sunlight and sky as one row keeps
/// their times in step; three separate keyframe tables would not.
struct SunlightValue {
	float normTime;
	glm::vec4 ambient;
	glm::vec4 sunIntensity;
	glm::vec4 background;
	float maxIntensity; ///< the shader divides by this: HDR -> LDR
};

/// gltut's HDR lighting environment (Tut 12/HDRLighting.cpp). Daytime ambient
/// and sun are the LDR values times three, with maxIntensity 3.0 to match, so
/// a sunlit surface at noon renders exactly as it did in LDR. The point lights
/// are scaled the same way but maxIntensity is *not* held at 3.0 for them: it
/// falls to 1.0 at night, and that is where they become three times brighter
/// without a single change to their own values.
constexpr std::array<SunlightValue, 7> kSunlightValues{
    {
        {
            .normTime = 0.0F / 24.0F,
            .ambient = {0.6F, 0.6F, 0.6F, 1.0F},
            .sunIntensity = {1.8F, 1.8F, 1.8F, 1.0F},
            .background = kSkyDaylightColor,
            .maxIntensity = 3.0F,
        },
        {
            .normTime = 4.5F / 24.0F,
            .ambient = {0.6F, 0.6F, 0.6F, 1.0F},
            .sunIntensity = {1.8F, 1.8F, 1.8F, 1.0F},
            .background = kSkyDaylightColor,
            .maxIntensity = 3.0F,
        },
        {
            // dusk
            .normTime = 6.5F / 24.0F,
            .ambient = {0.225F, 0.075F, 0.075F, 1.0F},
            .sunIntensity = {0.45F, 0.15F, 0.15F, 1.0F},
            .background = {0.5F, 0.1F, 0.1F, 1.0F},
            .maxIntensity = 1.5F,
        },
        {
            // night begins
            .normTime = 8.0F / 24.0F,
            .ambient = {0.0F, 0.0F, 0.0F, 1.0F},
            .sunIntensity = {0.0F, 0.0F, 0.0F, 1.0F},
            .background = {0.0F, 0.0F, 0.0F, 1.0F},
            .maxIntensity = 1.0F,
        },
        {
            // night ends
            .normTime = 18.0F / 24.0F,
            .ambient = {0.0F, 0.0F, 0.0F, 1.0F},
            .sunIntensity = {0.0F, 0.0F, 0.0F, 1.0F},
            .background = {0.0F, 0.0F, 0.0F, 1.0F},
            .maxIntensity = 1.0F,
        },
        {
            // dawn
            .normTime = 19.5F / 24.0F,
            .ambient = {0.225F, 0.075F, 0.075F, 1.0F},
            .sunIntensity = {0.45F, 0.15F, 0.15F, 1.0F},
            .background = {0.5F, 0.1F, 0.1F, 1.0F},
            .maxIntensity = 1.5F,
        },
        {
            .normTime = 20.5F / 24.0F,
            .ambient = {0.6F, 0.6F, 0.6F, 1.0F},
            .sunIntensity = {1.8F, 1.8F, 1.8F, 1.0F},
            .background = kSkyDaylightColor,
            .maxIntensity = 3.0F,
        },
    },
};

/// Pulls one column out of kSunlightValues as keyframes. The keyframe type
/// follows whatever the projection returns, so the same helper serves the
/// vec4 colours and the float maxIntensity.
template<class Projection>
[[nodiscard]] auto sunColumn(Projection projection) {
	using Value = std::invoke_result_t<Projection, const SunlightValue&>;
	std::array<glc::Keyframe<Value>, kSunlightValues.size()> keyframes{};
	for (std::size_t i = 0; i < kSunlightValues.size(); ++i) {
		const SunlightValue& row = kSunlightValues[i];
		keyframes[i] = {.time = row.normTime, .value = projection(row)};
	}
	return keyframes;
}

// Point light paths, world space. Uneven by hand, which is exactly why they are
// driven by a constant-velocity interpolator: weighting by waypoint index would
// make each light lurch through the short segments and crawl through the long
// ones.

/// A ring above the terrain.
constexpr std::array kRingPath{
    glm::vec3{-50.0F, 30.0F, 70.0F},  glm::vec3{-70.0F, 30.0F, 50.0F},
    glm::vec3{-70.0F, 30.0F, -50.0F}, glm::vec3{-50.0F, 30.0F, -70.0F},
    glm::vec3{50.0F, 30.0F, -70.0F},  glm::vec3{70.0F, 30.0F, -50.0F},
    glm::vec3{70.0F, 30.0F, 50.0F},   glm::vec3{50.0F, 30.0F, 70.0F},
};

/// Right-side light: a rising spiral, then a second one further back.
constexpr std::array kRightPath{
    glm::vec3{100.0F, 6.0F, 75.0F},   glm::vec3{90.0F, 8.0F, 90.0F},
    glm::vec3{75.0F, 10.0F, 100.0F},  glm::vec3{60.0F, 12.0F, 90.0F},
    glm::vec3{50.0F, 14.0F, 75.0F},   glm::vec3{60.0F, 16.0F, 60.0F},
    glm::vec3{75.0F, 18.0F, 50.0F},   glm::vec3{90.0F, 20.0F, 60.0F},
    glm::vec3{100.0F, 22.0F, 75.0F},  glm::vec3{90.0F, 24.0F, 90.0F},
    glm::vec3{75.0F, 26.0F, 100.0F},  glm::vec3{60.0F, 28.0F, 90.0F},
    glm::vec3{50.0F, 30.0F, 75.0F},   glm::vec3{105.0F, 9.0F, -70.0F},
    glm::vec3{105.0F, 10.0F, -90.0F}, glm::vec3{72.0F, 20.0F, -90.0F},
    glm::vec3{72.0F, 22.0F, -70.0F},  glm::vec3{105.0F, 32.0F, -70.0F},
    glm::vec3{105.0F, 34.0F, -90.0F}, glm::vec3{72.0F, 44.0F, -90.0F},
};

/// Left-side light: three separate circuits strung together.
constexpr std::array kLeftPath{
    glm::vec3{-7.0F, 35.0F, 1.0F},    glm::vec3{8.0F, 40.0F, -14.0F},
    glm::vec3{-7.0F, 45.0F, -29.0F},  glm::vec3{-22.0F, 50.0F, -14.0F},
    glm::vec3{-7.0F, 55.0F, 1.0F},    glm::vec3{8.0F, 60.0F, -14.0F},
    glm::vec3{-7.0F, 65.0F, -29.0F},  glm::vec3{-83.0F, 30.0F, -92.0F},
    glm::vec3{-98.0F, 27.0F, -77.0F}, glm::vec3{-83.0F, 24.0F, -62.0F},
    glm::vec3{-68.0F, 21.0F, -77.0F}, glm::vec3{-83.0F, 18.0F, -92.0F},
    glm::vec3{-98.0F, 15.0F, -77.0F}, glm::vec3{-50.0F, 8.0F, 25.0F},
    glm::vec3{-59.5F, 4.0F, 65.0F},   glm::vec3{-59.5F, 4.0F, 78.0F},
    glm::vec3{-45.0F, 4.0F, 82.0F},   glm::vec3{-40.0F, 4.0F, 50.0F},
    glm::vec3{-70.0F, 20.0F, 40.0F},  glm::vec3{-60.0F, 20.0F, 90.0F},
    glm::vec3{-40.0F, 25.0F, 90.0F},
};

} // namespace

// CycleTimer has no default constructor -- a zero-length cycle has no
// meaningful alpha, so the duration is a constructor precondition. That forces
// the timers into the member initializer list rather than the body.
LightManager::LightManager()
    : pointTimers_{glc::CycleTimer{15.0F}, glc::CycleTimer{25.0F}, glc::CycleTimer{15.0F}},
      // HDR values: three times the LDR ones, like the sun. See kSunlightValues
      // for why that leaves them unchanged by day and brighter by night.
      pointIntensity_{
          glm::vec4{0.6F, 0.6F, 0.6F, 1.0F},
          glm::vec4{0.0F, 0.0F, 0.7F, 1.0F},
          glm::vec4{0.7F, 0.0F, 0.0F, 1.0F},
      } {
	ambientInterpolator_.setKeyframes(sunColumn([](const SunlightValue& v) { return v.ambient; }));
	sunIntensityInterpolator_.setKeyframes(
	    sunColumn([](const SunlightValue& v) { return v.sunIntensity; }));
	backgroundInterpolator_.setKeyframes(
	    sunColumn([](const SunlightValue& v) { return v.background; }));
	maxIntensityInterpolator_.setKeyframes(
	    sunColumn([](const SunlightValue& v) { return v.maxIntensity; }));

	paths_[0].setWaypoints(kRingPath);
	paths_[1].setWaypoints(kRightPath);
	paths_[2].setWaypoints(kLeftPath);
}

void LightManager::update(float deltaSeconds) {
	sunTimer_.update(deltaSeconds);
	for (glc::CycleTimer& timer : pointTimers_) {
		timer.update(deltaSeconds);
	}
}

glm::vec3 LightManager::sunDirection() const {
	const float angle = sunTimer_.alpha() * 2.0F * std::numbers::pi_v<float>;
	const glm::vec4 overhead{std::sin(angle), std::cos(angle), 0.0F, 0.0F};

	// Tilted off vertical so the sun never passes exactly overhead. At perfect
	// noon every flat patch of terrain shares one angle of incidence and the
	// whole scene reads as unshaded.
	//
	// gltut writes `glm::rotate(mat4(1), 5.0f, ...)`: glm took degrees back
	// then, and takes radians now.
	const glm::mat4 tilt =
	    glm::rotate(glm::mat4{1.0F}, glm::radians(5.0F), glm::vec3{0.0F, 1.0F, 0.0F});
	return glm::vec3{tilt * overhead};
}

float LightManager::attenuation() const {
	return 1.0F / (halfBrightnessDistance_ * halfBrightnessDistance_);
}

glm::vec4 LightManager::sunIntensity() const {
	return sunIntensityInterpolator_.interpolate(sunTimer_.alpha());
}

glm::vec3 LightManager::pointLightPosition(std::size_t index) const {
	return paths_[index].interpolate(pointTimers_[index].alpha());
}

glm::vec4 LightManager::pointLightIntensity(std::size_t index) const {
	return pointIntensity_[index];
}

LightBlock LightManager::toBlock(const glm::mat4& worldToCamera) const {
	LightBlock result{};
	result.ambientIntensity = ambientInterpolator_.interpolate(sunTimer_.alpha());
	result.maxIntensity = maxIntensityInterpolator_.interpolate(sunTimer_.alpha());
	result.lightAttenuation = attenuation();

	// One multiply serves both kinds. w = 0 zeroes the view matrix's
	// translation column, leaving a pure rotation of the direction; w = 1 lets
	// the translation apply. No branch needed on this side.
	result.lights[0].cameraSpacePos = worldToCamera * glm::vec4{sunDirection(), 0.0F};
	result.lights[0].intensity = sunIntensity();

	for (std::size_t i = 0; i < kNumberOfPointLights; ++i) {
		result.lights[i + 1].cameraSpacePos =
		    worldToCamera * glm::vec4{pointLightPosition(i), 1.0F};
		result.lights[i + 1].intensity = pointLightIntensity(i);
	}

	return result;
}

glm::vec4 LightManager::backgroundColor() const {
	return backgroundInterpolator_.interpolate(sunTimer_.alpha());
}

float LightManager::sunAlpha() const {
	return sunTimer_.alpha();
}

float LightManager::sunTime() const {
	return sunTimer_.alpha() * 24.0F;
}

void LightManager::setSunTime(float hours) {
	sunTimer_.setAlpha(hours / 24.0F);
}

namespace {

/// True when `scope` covers the sun's clock.
[[nodiscard]] bool includesSun(TimerScope scope) {
	return scope != TimerScope::PointLights;
}

/// True when `scope` covers the point lights' clocks.
[[nodiscard]] bool includesPointLights(TimerScope scope) {
	return scope != TimerScope::Sun;
}

} // namespace

void LightManager::setPaused(TimerScope scope, bool paused) {
	if (includesSun(scope)) {
		sunTimer_.setPaused(paused);
	}
	if (includesPointLights(scope)) {
		for (glc::CycleTimer& timer : pointTimers_) {
			timer.setPaused(paused);
		}
	}
}

bool LightManager::isPaused(TimerScope scope) const {
	const bool sunPaused = !includesSun(scope) || sunTimer_.isPaused();
	const bool pointsPaused =
	    !includesPointLights(scope) ||
	    std::ranges::all_of(pointTimers_, [](const glc::CycleTimer& t) { return t.isPaused(); });
	return sunPaused && pointsPaused;
}

void LightManager::togglePause(TimerScope scope) {
	// For All this reads "if anything is still running, freeze the lot".
	setPaused(scope, !isPaused(scope));
}

void LightManager::rewind(TimerScope scope, float seconds) {
	fastForward(scope, -seconds);
}

void LightManager::fastForward(TimerScope scope, float seconds) {
	if (includesSun(scope)) {
		sunTimer_.fastForward(seconds);
	}
	if (includesPointLights(scope)) {
		for (glc::CycleTimer& timer : pointTimers_) {
			timer.fastForward(seconds);
		}
	}
}
