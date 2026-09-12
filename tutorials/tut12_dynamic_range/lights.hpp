#pragma once

#include <glcore/cycle_timer.hpp>
#include <glcore/interpolator.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <cstddef>

/// One light as the shader sees it. `cameraSpacePos.w` selects the kind:
/// 0 makes the xyz a direction (the sun, unattenuated), 1 makes it a position.
struct PerLight {
	glm::vec4 cameraSpacePos;
	glm::vec4 intensity;
};

constexpr std::size_t kNumberOfPointLights = 3;

/// std140 layout. Nothing here is padded automatically: glm's vec4 has an
/// alignment of 4, not 16, so every byte of conformance is manual and the
/// static_assert below is the only thing checking it.
struct LightBlock {
	glm::vec4 ambientIntensity;
	float lightAttenuation;
	std::array<float, 3> padding; ///< pushes lights[0] to offset 32, as std140 requires
	std::array<PerLight, kNumberOfPointLights + 1> lights; ///< [0] is the sun
};
static_assert(sizeof(LightBlock) == 160);

/// The day/night cycle: one sun on a 30-second loop plus three point lights on
/// loops of their own, all authored in world space and converted on demand.
///
/// This is gltut's LightManager. Nothing about the lights' geometry is stored --
/// the sun's direction is a function of its timer, and each point light's
/// position is a function of its path and its timer. Only the tables are data.
class LightManager {
public:
	LightManager();

	/// Advances every timer. Deltas come from App::onUpdate.
	void update(float deltaSeconds);

	/// Builds the block the GPU wants, converting world-space lights into
	/// camera space. The only place the `w` component is decided.
	[[nodiscard]] LightBlock toBlock(const glm::mat4& worldToCamera) const;

	[[nodiscard]] glm::vec4 backgroundColor() const;

	/// Time of day in hours, [0, 24). Note that hour 0 is *noon*: the sun's
	/// direction at alpha 0 points straight up, and the dark stretch of the
	/// keyframe table straddles hour 12.
	[[nodiscard]] float sunTime() const;

	[[nodiscard]] float halfBrightnessDistance() const noexcept {
		return halfBrightnessDistance_;
	}
	void setHalfBrightnessDistance(float distance) noexcept {
		halfBrightnessDistance_ = distance;
	}

private:
	/// World-space unit vector pointing *toward* the sun. Derived from the sun
	/// timer, never stored.
	[[nodiscard]] glm::vec3 sunDirection() const;

	/// k in `I / (1 + k * d*d)`, derived so intensity halves at
	/// halfBrightnessDistance_.
	[[nodiscard]] float attenuation() const;

	glc::CycleTimer sunTimer_{30.0F}; ///< one full day per 30 seconds

	glc::TimedLinearInterpolator<glm::vec4> ambientInterpolator_;
	glc::TimedLinearInterpolator<glm::vec4> sunIntensityInterpolator_;
	glc::TimedLinearInterpolator<glm::vec4> backgroundInterpolator_;

	/// Constant 1.0 until the HDR stage, where dividing by it stops being a
	/// no-op. Present now so that stage is a data change, not a code change.
	glc::TimedLinearInterpolator<float> maxIntensityInterpolator_;

	std::array<glc::ConstVelLinearInterpolator<glm::vec3>, kNumberOfPointLights> paths_;
	std::array<glc::CycleTimer, kNumberOfPointLights> pointTimers_;
	std::array<glm::vec4, kNumberOfPointLights> pointIntensity_;

	float halfBrightnessDistance_{70.0F};
};
