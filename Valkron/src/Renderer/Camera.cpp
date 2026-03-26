#include "Renderer/Camera.hpp"

#include "glm/gtc/matrix_transform.hpp"

namespace Valkron {

    Camera::Camera()
        : m_position(0.0f, 0.0f, 2.0f)
        , m_target(0.0f, 0.0f, 0.0f)
        , m_up(0.0f, 1.0f, 0.0f)
        , m_fovDegrees(45.0f)
        , m_nearPlane(0.1f)
        , m_farPlane(100.0f) {}

    glm::mat4 Camera::getViewMatrix() const {
        return glm::lookAt(m_position, m_target, m_up);
    }

    glm::mat4 Camera::getProjectionMatrix(float aspectRatio) const {
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
        m_fovDegrees = fovDegrees;
        m_nearPlane = nearPlane;
        m_farPlane = farPlane;
    }

}