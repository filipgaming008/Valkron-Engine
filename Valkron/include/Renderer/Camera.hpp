#pragma once

#include "Core/Core.hpp"
#include "glm/glm.hpp"

namespace Valkron {

    class VALKRON_API Camera {
        public:
            Camera();

            glm::mat4 getViewMatrix() const;
            glm::mat4 getProjectionMatrix(float aspectRatio) const;

            void setPosition(const glm::vec3& position);
            void setTarget(const glm::vec3& target);
            void setUp(const glm::vec3& up);
            void setPerspective(float fovDegrees, float nearPlane, float farPlane);

        private:
            glm::vec3 m_position;
            glm::vec3 m_target;
            glm::vec3 m_up;

            float m_fovDegrees;
            float m_nearPlane;
            float m_farPlane;
    };

}