#pragma once

#include "Core/Core.hpp"

#include "glm/glm.hpp"

namespace Valkron {

    enum class CameraType {
        Perspective,
        Orthographic
    };

    class VALKRON_API Camera {
        public:
            explicit Camera(CameraType type = CameraType::Perspective);

            CameraType getType() const;
            void setType(CameraType type);

            glm::mat4 getViewMatrix() const;
            glm::mat4 getProjectionMatrix(float aspectRatio) const;

            void setPosition(const glm::vec3& position);
            void setTarget(const glm::vec3& target);
            void setUp(const glm::vec3& up);
            void setPerspective(float fovDegrees, float nearPlane, float farPlane);
            void setOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane);
        private:
            CameraType m_type;
            glm::vec3 m_position;
            glm::vec3 m_target;
            glm::vec3 m_up;

            float m_fovDegrees;
            float m_orthoLeft;
            float m_orthoRight;
            float m_orthoBottom;
            float m_orthoTop;
            float m_nearPlane;
            float m_farPlane;
    };

}