#pragma once

#include "Core/Core.hpp"
#include "Renderer/Camera.hpp"

#include "glm/mat4x4.hpp"
#include "glm/vec3.hpp"

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

#include <string>
#include <vector>

namespace Valkron {

    using GLFWwindow = ::GLFWwindow;

    struct VALKRON_API SceneModelInstance {
        std::string modelName;
        std::vector<int> modelMeshIndices;
        bool applyModelNodeTransforms = true;
        glm::mat4 transform{1.0f};
        bool selected = false;
        bool hasShaderComponent = false;
        std::string shaderName;
        glm::vec3 pbrAlbedoColor{1.0f, 1.0f, 1.0f};
        float pbrMetallic = 0.0f;
        float pbrRoughness = 0.55f;
        float pbrAmbientOcclusion = 1.0f;
        std::string pbrDiffuseTexture;
        std::string pbrAlbedoTexture;
        std::string pbrAlphaTexture;
        std::string pbrNormalTexture;
        std::string pbrMetallicTexture;
        std::string pbrRoughnessTexture;
        std::string pbrAOTexture;
    };

    enum class RenderLightType {
        Directional = 0,
        Point = 1
    };

    struct VALKRON_API SceneLightState {
        RenderLightType type = RenderLightType::Directional;
        glm::vec3 direction{-0.40f, -1.00f, -0.30f};
        glm::vec3 position{3.0f, 4.0f, 3.0f};
        glm::vec3 color{1.0f, 1.0f, 0.96f};
        float intensity = 1.15f;
        float ambientStrength = 0.24f;
        float range = 12.0f;
        bool enabled = true;
    };

    class VALKRON_API Renderer {
        public:
            static void init(GLFWwindow* window);
            static void shutdown();
            static void beginFrame();
            static void endFrame();
            static void setCameraType(CameraType type);
            static void setCameraLookAt(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f));
            static void setModelTransform(const glm::vec3& position, const glm::vec3& rotationDegrees, const glm::vec3& scale);
            static void setSceneModelInstances(const std::vector<SceneModelInstance>& modelInstances);
            static void setSceneEntityTransforms(const std::vector<glm::mat4>& entityTransforms, int selectedEntityIndex);
            static void setLightEntityPositions(const std::vector<glm::vec3>& lightPositions);
            static void setSceneLight(const SceneLightState& lightState);
            static void setDirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity, float ambientStrength);
            static glm::vec3 getCameraPosition();
            static glm::vec3 getCameraTarget();
            static glm::vec3 getCameraUp();
            static glm::mat4 getCameraViewMatrix();
            static glm::mat4 getCameraProjectionMatrix();
            static void onWindowResize(int width, int height);
            static void setViewportSize(int width, int height);
            static unsigned int getFrameTextureID();
            static unsigned int getSceneFrameTextureID();
            static unsigned int getGameFrameTextureID();
            static int getViewportWidth();
            static int getViewportHeight();
    };

}