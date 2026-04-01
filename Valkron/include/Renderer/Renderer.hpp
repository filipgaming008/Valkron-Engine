#pragma once

#include "Core/Core.hpp"
#include "Renderer/Camera.hpp"

#include "glm/mat4x4.hpp"
#include "glm/vec3.hpp"

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

#include <vector>

namespace Valkron {

    using GLFWwindow = ::GLFWwindow;

    class VALKRON_API Renderer {
        public:
            static void init(GLFWwindow* window);
            static void shutdown();
            static void beginFrame();
            static void endFrame();
            static void setCameraType(CameraType type);
            static void setCameraLookAt(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f));
            static void setModelTransform(const glm::vec3& position, const glm::vec3& rotationDegrees, const glm::vec3& scale);
            static void setSceneEntityTransforms(const std::vector<glm::mat4>& entityTransforms, int selectedEntityIndex);
            static void setLightEntityPositions(const std::vector<glm::vec3>& lightPositions);
            static glm::vec3 getCameraPosition();
            static glm::vec3 getCameraTarget();
            static glm::vec3 getCameraUp();
            static glm::mat4 getCameraViewMatrix();
            static glm::mat4 getCameraProjectionMatrix();
            static void onWindowResize(int width, int height);
            static void setViewportSize(int width, int height);
            static unsigned int getFrameTextureID();
            static int getViewportWidth();
            static int getViewportHeight();
    };

}