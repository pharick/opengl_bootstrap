#pragma once

#include <glcore/input.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace glc {

inline constexpr glm::vec3 kWorldUp{0.0F, 1.0F, 0.0F};

/// View and projection, kept together and cached.
///
/// The tutorials in the book compute a look-at matrix by hand at first; this is
/// the same thing, factored out so chapters 6+ do not repeat it.
class Camera {
public:
	Camera();

	void setPerspective(float fovYDegrees, float aspect, float zNear, float zFar);
	void setAspect(float aspect);
	void setFovY(float degrees);
	void setClipPlanes(float zNear, float zFar);

	void setView(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& up = kWorldUp);

	[[nodiscard]] const glm::mat4& view() const noexcept {
		return view_;
	}
	[[nodiscard]] const glm::mat4& projection() const noexcept {
		return projection_;
	}
	[[nodiscard]] glm::mat4 viewProjection() const {
		return projection_ * view_;
	}

	[[nodiscard]] const glm::vec3& eye() const noexcept {
		return eye_;
	}
	[[nodiscard]] const glm::vec3& target() const noexcept {
		return target_;
	}
	[[nodiscard]] glm::vec3 forward() const;
	[[nodiscard]] glm::vec3 right() const;

	[[nodiscard]] float fovY() const noexcept {
		return fovYDegrees_;
	}
	[[nodiscard]] float aspect() const noexcept {
		return aspect_;
	}
	[[nodiscard]] float zNear() const noexcept {
		return zNear_;
	}
	[[nodiscard]] float zFar() const noexcept {
		return zFar_;
	}

private:
	void rebuildProjection();

	glm::mat4 view_{1.0F};
	glm::mat4 projection_{1.0F};

	glm::vec3 eye_{0.0F, 0.0F, 5.0F};
	glm::vec3 target_{0.0F, 0.0F, 0.0F};
	glm::vec3 up_{kWorldUp};

	float fovYDegrees_ = 45.0F;
	float aspect_ = 16.0F / 9.0F;
	float zNear_ = 0.1F;
	float zFar_ = 1000.0F;
};

/// Mouse-driven orbit camera, filling the role of gltut's ViewPole.
///
///   left drag           orbit
///   middle drag,
///   or shift+left drag  pan
///   scroll wheel        dolly in/out
class OrbitController {
public:
	struct Settings {
		float orbitDegreesPerPixel = 0.3F;
		float panUnitsPerPixel = 0.0025F; ///< scaled by distance
		float zoomPerScrollNotch = 0.12F; ///< fraction of current distance
		float minDistance = 0.05F;
		float maxDistance = 5000.0F;
		float minPitchDegrees = -89.0F;
		float maxPitchDegrees = 89.0F;
	};

	explicit OrbitController(glm::vec3 target = glm::vec3{0.0F}, float distance = 5.0F,
	                         float yawDegrees = 45.0F, float pitchDegrees = 25.0F);

	/// Reads the input and writes the resulting view into `camera`.
	void update(Camera& camera, const Input& input);

	void setTarget(const glm::vec3& target) noexcept {
		target_ = target;
	}
	void setDistance(float distance);
	void setAngles(float yawDegrees, float pitchDegrees);

	[[nodiscard]] const glm::vec3& target() const noexcept {
		return target_;
	}
	[[nodiscard]] float distance() const noexcept {
		return distance_;
	}
	[[nodiscard]] float yaw() const noexcept {
		return yawDegrees_;
	}
	[[nodiscard]] float pitch() const noexcept {
		return pitchDegrees_;
	}

	[[nodiscard]] Settings& settings() noexcept {
		return settings_;
	}

private:
	[[nodiscard]] glm::vec3 computeEye() const;

	glm::vec3 target_;
	float distance_;
	float yawDegrees_;
	float pitchDegrees_;
	Settings settings_{};
};

/// First-person camera: hold right mouse to look, WASD to move, Q/E for
/// down/up, shift to sprint.
class FlyController {
public:
	struct Settings {
		float degreesPerPixel = 0.15F;
		float unitsPerSecond = 4.0F;
		float sprintMultiplier = 4.0F;
	};

	explicit FlyController(glm::vec3 position = glm::vec3{0.0F, 0.0F, 5.0F},
	                       float yawDegrees = -90.0F, float pitchDegrees = 0.0F);

	void update(Camera& camera, const Input& input, float deltaSeconds);

	void setPosition(const glm::vec3& position) noexcept {
		position_ = position;
	}
	[[nodiscard]] const glm::vec3& position() const noexcept {
		return position_;
	}

	[[nodiscard]] Settings& settings() noexcept {
		return settings_;
	}

private:
	[[nodiscard]] glm::vec3 forward() const;

	glm::vec3 position_;
	float yawDegrees_;
	float pitchDegrees_;
	Settings settings_{};
};

} // namespace glc
