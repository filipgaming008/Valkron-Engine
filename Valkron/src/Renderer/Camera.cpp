#include "Renderer/Camera.hpp"

#include "glm/gtc/matrix_transform.hpp"

namespace Valkron {

Camera::Camera(CameraType type)
    : m_type(type), m_position(0.0f, 0.0f, 2.0f), m_target(0.0f, 0.0f, 0.0f), m_up(0.0f, 1.0f, 0.0f),
      m_fovDegrees(45.0f), m_orthoLeft(-1.0f), m_orthoRight(1.0f), m_orthoBottom(-1.0f), m_orthoTop(1.0f),
      m_nearPlane(0.1f), m_farPlane(100.0f) {}

CameraType Camera::getType() const {
    return m_type;
}

void Camera::setType(CameraType type) {
    m_type = type;
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(m_position, m_target, m_up);
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio) const {
    if (m_type == CameraType::Orthographic) {
        return glm::ortho(m_orthoLeft, m_orthoRight, m_orthoBottom, m_orthoTop, m_nearPlane, m_farPlane);
    }

    return glm::perspective(glm::radians(m_fovDegrees), aspectRatio, m_nearPlane, m_farPlane);
}

void Camera::setPosition(const glm::vec3& position) {
    m_position = position;
}

void Camera::setTarget(const glm::vec3& target) {
    m_target = target;
}

void Camera::setUp(const glm::vec3& up) {
    m_up = up;
}

void Camera::setPerspective(float fovDegrees, float nearPlane, float farPlane) {
    m_type = CameraType::Perspective;
    m_fovDegrees = fovDegrees;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
}

void Camera::setOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
    m_type = CameraType::Orthographic;
    m_orthoLeft = left;
    m_orthoRight = right;
    m_orthoBottom = bottom;
    m_orthoTop = top;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
}

} // namespace Valkron