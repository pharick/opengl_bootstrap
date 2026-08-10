#include <glcore/camera.hpp>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace glc {
namespace {

float clampf(float value, float low, float high) {
	return std::clamp(value, low, high);
}

} // namespace

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------

Camera::Camera() {
	setView(eye_, target_, up_);
	rebuildProjection();
}

void Camera::rebuildProjection() {
	projection_ = glm::perspective(glm::radians(fovYDegrees_), aspect_, zNear_, zFar_);
}

void Camera::setPerspective(float fovYDegrees, float aspect, float zNear, float zFar) {
	fovYDegrees_ = fovYDegrees;
	aspect_ = aspect;
	zNear_ = zNear;
	zFar_ = zFar;
	rebuildProjection();
}

void Camera::setAspect(float aspect) {
	// A minimized window reports a zero-height framebuffer; a zero aspect would
	// make the projection matrix non-invertible.
	aspect_ = aspect > 0.0F ? aspect : 1.0F;
	rebuildProjection();
}

void Camera::setFovY(float degrees) {
	fovYDegrees_ = degrees;
	rebuildProjection();
}

void Camera::setClipPlanes(float zNear, float zFar) {
	zNear_ = zNear;
	zFar_ = zFar;
	rebuildProjection();
}

void Camera::setView(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& up) {
	eye_ = eye;
	target_ = target;
	up_ = up;
	view_ = glm::lookAt(eye_, target_, up_);
}

glm::vec3 Camera::forward() const {
	return glm::normalize(target_ - eye_);
}

glm::vec3 Camera::right() const {
	return glm::normalize(glm::cross(forward(), up_));
}

// ---------------------------------------------------------------------------
// OrbitController
// ---------------------------------------------------------------------------

OrbitController::OrbitController(glm::vec3 target, float distance, float yawDegrees,
                                 float pitchDegrees)
    : target_{target}, distance_{distance}, yawDegrees_{yawDegrees}, pitchDegrees_{pitchDegrees} {}

void OrbitController::setDistance(float distance) {
	distance_ = clampf(distance, settings_.minDistance, settings_.maxDistance);
}

void OrbitController::setAngles(float yawDegrees, float pitchDegrees) {
	yawDegrees_ = yawDegrees;
	pitchDegrees_ = clampf(pitchDegrees, settings_.minPitchDegrees, settings_.maxPitchDegrees);
}

glm::vec3 OrbitController::computeEye() const {
	const float yaw = glm::radians(yawDegrees_);
	const float pitch = glm::radians(pitchDegrees_);
	const float cosPitch = std::cos(pitch);
	return target_ + distance_ * glm::vec3{cosPitch * std::sin(yaw), std::sin(pitch),
	                                       cosPitch * std::cos(yaw)};
}

void OrbitController::update(Camera& camera, const Input& input) {
	const glm::vec2 drag = input.mouseDelta();

	const bool shift = input.keyDown(GLFW_KEY_LEFT_SHIFT) || input.keyDown(GLFW_KEY_RIGHT_SHIFT);
	const bool orbiting = input.mouseDown(GLFW_MOUSE_BUTTON_LEFT) && !shift;
	const bool panning = input.mouseDown(GLFW_MOUSE_BUTTON_MIDDLE) ||
	                     (input.mouseDown(GLFW_MOUSE_BUTTON_LEFT) && shift);

	if (orbiting) {
		setAngles(yawDegrees_ - (drag.x * settings_.orbitDegreesPerPixel),
		          pitchDegrees_ + (drag.y * settings_.orbitDegreesPerPixel));
	}

	if (panning) {
		// Pan in the camera's own plane, scaled by distance so the target keeps
		// up with the cursor at any zoom level.
		const glm::vec3 eye = computeEye();
		const glm::vec3 forward = glm::normalize(target_ - eye);
		const glm::vec3 right = glm::normalize(glm::cross(forward, kWorldUp));
		const glm::vec3 up = glm::cross(right, forward);
		const float scale = settings_.panUnitsPerPixel * distance_;
		target_ += (-drag.x * scale * right) + (drag.y * scale * up);
	}

	if (const float scroll = input.scrollDelta(); scroll != 0.0F) {
		setDistance(distance_ * (1.0F - (scroll * settings_.zoomPerScrollNotch)));
	}

	camera.setView(computeEye(), target_, kWorldUp);
}

// ---------------------------------------------------------------------------
// FlyController
// ---------------------------------------------------------------------------

FlyController::FlyController(glm::vec3 position, float yawDegrees, float pitchDegrees)
    : position_{position}, yawDegrees_{yawDegrees}, pitchDegrees_{pitchDegrees} {}

glm::vec3 FlyController::forward() const {
	const float yaw = glm::radians(yawDegrees_);
	const float pitch = glm::radians(pitchDegrees_);
	const float cosPitch = std::cos(pitch);
	return glm::normalize(
	    glm::vec3{cosPitch * std::cos(yaw), std::sin(pitch), cosPitch * std::sin(yaw)});
}

void FlyController::update(Camera& camera, const Input& input, float deltaSeconds) {
	if (input.mouseDown(GLFW_MOUSE_BUTTON_RIGHT)) {
		const glm::vec2 drag = input.mouseDelta();
		yawDegrees_ += drag.x * settings_.degreesPerPixel;
		pitchDegrees_ = clampf(pitchDegrees_ - (drag.y * settings_.degreesPerPixel), -89.0F, 89.0F);
	}

	const glm::vec3 ahead = forward();
	const glm::vec3 right = glm::normalize(glm::cross(ahead, kWorldUp));

	glm::vec3 motion{0.0F};
	if (input.keyDown(GLFW_KEY_W)) {
		motion += ahead;
	}
	if (input.keyDown(GLFW_KEY_S)) {
		motion -= ahead;
	}
	if (input.keyDown(GLFW_KEY_D)) {
		motion += right;
	}
	if (input.keyDown(GLFW_KEY_A)) {
		motion -= right;
	}
	if (input.keyDown(GLFW_KEY_E)) {
		motion += kWorldUp;
	}
	if (input.keyDown(GLFW_KEY_Q)) {
		motion -= kWorldUp;
	}

	if (glm::dot(motion, motion) > 0.0F) {
		const bool sprint =
		    input.keyDown(GLFW_KEY_LEFT_SHIFT) || input.keyDown(GLFW_KEY_RIGHT_SHIFT);
		const float speed = settings_.unitsPerSecond * (sprint ? settings_.sprintMultiplier : 1.0F);
		position_ += glm::normalize(motion) * speed * deltaSeconds;
	}

	camera.setView(position_, position_ + ahead, kWorldUp);
}

} // namespace glc
