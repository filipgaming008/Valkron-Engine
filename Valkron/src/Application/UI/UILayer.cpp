#include "Application/UI/UILayer.hpp"

#include "Application/UI/RuntimeAssetImportService.hpp"
#include "Core/FileSystem.hpp"
#include "Core/Log.hpp"
#include "Engine/AssetManager.hpp"
#include "Engine/AssetLoader.hpp"
#include "Event/Event.hpp"
#include "Renderer/Model.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/Texture.hpp"
#include "Window/Window.hpp"

#include "imgui.h"
#include "imgui_internal.h"
#include "ImGuizmo.h"

#include "glm/glm.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/constants.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/trigonometric.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <unordered_set>

namespace Valkron {

    const char* sceneStateToString(SceneState state) {
        switch (state) {
            case SceneState::Edit:
                return "Edit";
            case SceneState::Play:
                return "Play";
            case SceneState::Pause:
                return "Pause";
            default:
                return "Unknown";
        }
    }

    const char* cameraTypeToString(CameraType type) {
        switch (type) {
            case CameraType::Perspective:
                return "Perspective";
            case CameraType::Orthographic:
                return "Orthographic";
            default:
                return "Unknown";
        }
    }

    glm::vec3 computeOrbitOffset(float distance, float yawRadians, float pitchRadians) {
        const float cosPitch = glm::cos(pitchRadians);
        return glm::vec3(
            distance * cosPitch * glm::sin(yawRadians),
            distance * glm::sin(pitchRadians),
            distance * cosPitch * glm::cos(yawRadians)
        );
    }

    std::string toLowercase(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    }

    std::string extractAssetFolderName(const SceneAsset& asset) {
        const std::filesystem::path assetPath(asset.path);
        const std::string folderName = assetPath.parent_path().filename().string();
        if (!folderName.empty()) {
            return folderName;
        }

        return "Root";
    }

    std::string normalizeFolderPath(const std::string& pathValue) {
        if (pathValue.empty()) {
            return {};
        }

        std::string normalized = std::filesystem::path(pathValue).lexically_normal().generic_string();
        if (normalized == ".") {
            return {};
        }

        if (normalized.starts_with("./")) {
            normalized = normalized.substr(2);
        }

        while (!normalized.empty() && normalized.back() == '/') {
            normalized.pop_back();
        }

        return normalized;
    }

    std::string getAssetFolderPath(const SceneAsset& asset) {
        const std::filesystem::path assetPath(asset.path);
        return normalizeFolderPath(assetPath.parent_path().generic_string());
    }

    std::string getFolderLeafName(const std::string& folderPath) {
        const std::string normalized = normalizeFolderPath(folderPath);
        if (normalized.empty()) {
            return "Root";
        }

        const std::filesystem::path pathValue(normalized);
        const std::string filename = pathValue.filename().string();
        if (!filename.empty()) {
            return filename;
        }

        return normalized;
    }

    std::string getParentFolderPath(const std::string& folderPath) {
        const std::string normalized = normalizeFolderPath(folderPath);
        if (normalized.empty()) {
            return {};
        }

        return normalizeFolderPath(std::filesystem::path(normalized).parent_path().generic_string());
    }

    std::optional<std::string> getDirectChildFolder(const std::string& currentFolder, const std::string& candidateFolder) {
        const std::string current = normalizeFolderPath(currentFolder);
        const std::string candidate = normalizeFolderPath(candidateFolder);
        if (candidate.empty()) {
            return std::nullopt;
        }

        if (current.empty()) {
            const std::size_t slashPos = candidate.find('/');
            if (slashPos == std::string::npos) {
                return candidate;
            }

            return candidate.substr(0, slashPos);
        }

        if (candidate == current) {
            return std::nullopt;
        }

        const std::string prefix = current + "/";
        if (!candidate.starts_with(prefix)) {
            return std::nullopt;
        }

        const std::string remainder = candidate.substr(prefix.size());
        if (remainder.empty()) {
            return std::nullopt;
        }

        const std::size_t slashPos = remainder.find('/');
        if (slashPos == std::string::npos) {
            return prefix + remainder;
        }

        return prefix + remainder.substr(0, slashPos);
    }

    std::string getAssetExtensionLowercase(const SceneAsset& asset) {
        return toLowercase(std::filesystem::path(asset.path).extension().string());
    }

    bool isModelSceneAsset(const SceneAsset& asset) {
        const std::string extension = getAssetExtensionLowercase(asset);
        return extension == ".obj" || extension == ".fbx" || extension == ".gltf" || extension == ".glb" || extension == ".dae" || extension == ".3ds";
    }

    const char* getAssetIconToken(const SceneAsset& asset) {
        const std::string extension = getAssetExtensionLowercase(asset);
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" || extension == ".tga" || extension == ".ppm") {
            return "IMG";
        }
        if (extension == ".vert" || extension == ".frag" || extension == ".vs" || extension == ".fs" || extension == ".glsl" || extension == ".comp") {
            return "SHD";
        }
        if (extension == ".obj" || extension == ".fbx" || extension == ".gltf" || extension == ".glb" || extension == ".dae" || extension == ".3ds") {
            return "MOD";
        }
        if (extension == ".lua") {
            return "LUA";
        }

        return "AST";
    }

    ImVec4 getAssetIconColor(const SceneAsset& asset) {
        const std::string extension = getAssetExtensionLowercase(asset);
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" || extension == ".tga" || extension == ".ppm") {
            return ImVec4(0.18f, 0.34f, 0.58f, 1.0f);
        }
        if (extension == ".vert" || extension == ".frag" || extension == ".vs" || extension == ".fs" || extension == ".glsl" || extension == ".comp") {
            return ImVec4(0.42f, 0.24f, 0.14f, 1.0f);
        }
        if (extension == ".obj" || extension == ".fbx" || extension == ".gltf" || extension == ".glb" || extension == ".dae" || extension == ".3ds") {
            return ImVec4(0.16f, 0.44f, 0.28f, 1.0f);
        }
        if (extension == ".lua") {
            return ImVec4(0.45f, 0.34f, 0.14f, 1.0f);
        }

        return ImVec4(0.24f, 0.24f, 0.30f, 1.0f);
    }

    bool isCameraEntity(const SceneEntity& entity) {
        return entity.type == SceneEntityType::Camera;
    }

    bool isLightEntity(const SceneEntity& entity) {
        return entity.type == SceneEntityType::Light;
    }

    std::optional<std::size_t> resolvePrimaryPlayCameraEntityIndex(const Scene& scene) {
        const std::vector<SceneEntity>& entities = scene.getEntityData();
        const std::optional<std::string> preferredCameraName = scene.getGameStateValue("PrimaryCameraEntity");

        if (preferredCameraName.has_value() && !preferredCameraName->empty()) {
            for (std::size_t i = 0; i < entities.size(); ++i) {
                if (isCameraEntity(entities[i]) && entities[i].name == preferredCameraName.value()) {
                    return i;
                }
            }
        }

        for (std::size_t i = 0; i < entities.size(); ++i) {
            if (isCameraEntity(entities[i])) {
                return i;
            }
        }

        return std::nullopt;
    }

    const char* getSceneEntityTypeDisplayName(SceneEntityType type) {
        switch (type) {
            case SceneEntityType::Camera:
                return "Camera";
            case SceneEntityType::Light:
                return "Light";
            case SceneEntityType::Generic:
            default:
                return "Generic";
        }
    }

    const char* getEntityCategoryToken(const SceneEntity& entity) {
        switch (entity.type) {
            case SceneEntityType::Camera:
                return "CAM";
            case SceneEntityType::Light:
                return "LGT";
            case SceneEntityType::Generic:
                return entity.modelAssetName.empty() ? "ENT" : "MOD";
            default:
                break;
        }

        return "ENT";
    }

    const char* getEntityIconToken(const SceneEntity& entity) {
        switch (entity.type) {
            case SceneEntityType::Camera:
                return "[@]";
            case SceneEntityType::Light:
                return "[*]";
            case SceneEntityType::Generic:
                return entity.modelAssetName.empty() ? "[ ]" : "[M]";
            default:
                break;
        }

        return "[ ]";
    }

    ImVec4 getEntityBadgeColor(const SceneEntity& entity) {
        switch (entity.type) {
            case SceneEntityType::Camera:
                return ImVec4(0.16f, 0.30f, 0.54f, 1.0f);
            case SceneEntityType::Light:
                return ImVec4(0.42f, 0.32f, 0.10f, 1.0f);
            case SceneEntityType::Generic:
                if (!entity.modelAssetName.empty()) {
                    return ImVec4(0.16f, 0.38f, 0.26f, 1.0f);
                }
            default:
                break;
        }

        return ImVec4(0.30f, 0.30f, 0.34f, 1.0f);
    }

    ImVec4 brightenColor(const ImVec4& color, float amount) {
        return ImVec4(
            std::clamp(color.x + amount, 0.0f, 1.0f),
            std::clamp(color.y + amount, 0.0f, 1.0f),
            std::clamp(color.z + amount, 0.0f, 1.0f),
            color.w
        );
    }

    glm::mat4 composeLocalTransformMatrix(const SceneTransform& transform) {
        const glm::vec3 combinedScale(
            std::max(0.01f, transform.scale.x * transform.size.x),
            std::max(0.01f, transform.scale.y * transform.size.y),
            std::max(0.01f, transform.scale.z * transform.size.z)
        );

        glm::mat4 matrix(1.0f);
        matrix = glm::translate(matrix, transform.position);
        matrix = glm::rotate(matrix, glm::radians(transform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        matrix = glm::rotate(matrix, glm::radians(transform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        matrix = glm::rotate(matrix, glm::radians(transform.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        matrix = glm::scale(matrix, combinedScale);
        return matrix;
    }

    glm::mat4 composeEntityWorldTransformMatrix(const std::vector<SceneEntity>& entities, std::size_t entityIndex) {
        if (entityIndex >= entities.size()) {
            return glm::mat4(1.0f);
        }

        glm::mat4 worldMatrix = composeLocalTransformMatrix(entities[entityIndex].transform);

        int parentIndex = entities[entityIndex].parentIndex;
        std::size_t safety = 0;
        while (parentIndex >= 0 && parentIndex < static_cast<int>(entities.size()) && safety < entities.size()) {
            worldMatrix = composeLocalTransformMatrix(entities[static_cast<std::size_t>(parentIndex)].transform) * worldMatrix;
            parentIndex = entities[static_cast<std::size_t>(parentIndex)].parentIndex;
            ++safety;
        }

        return worldMatrix;
    }

    glm::vec3 extractWorldPosition(const glm::mat4& worldMatrix) {
        return glm::vec3(worldMatrix[3]);
    }

    glm::vec3 extractForwardDirection(const glm::mat4& worldMatrix) {
        glm::vec3 forward = glm::mat3(worldMatrix) * glm::vec3(0.0f, 0.0f, -1.0f);
        if (glm::length(forward) < 0.0001f) {
            return glm::vec3(0.0f, 0.0f, -1.0f);
        }

        return glm::normalize(forward);
    }

    std::optional<ImVec2> projectWorldPositionToImage(
        const glm::vec3& worldPosition,
        const glm::mat4& viewMatrix,
        const glm::mat4& projectionMatrix,
        const ImVec2& imageRectMin,
        const ImVec2& imageRectMax
    ) {
        const glm::vec4 clipPosition = projectionMatrix * viewMatrix * glm::vec4(worldPosition, 1.0f);
        if (clipPosition.w <= 0.0001f) {
            return std::nullopt;
        }

        const glm::vec3 ndc = glm::vec3(clipPosition) / clipPosition.w;
        if (ndc.x < -1.05f || ndc.x > 1.05f || ndc.y < -1.05f || ndc.y > 1.05f || ndc.z < -1.0f || ndc.z > 1.0f) {
            return std::nullopt;
        }

        const float imageWidth = imageRectMax.x - imageRectMin.x;
        const float imageHeight = imageRectMax.y - imageRectMin.y;
        if (imageWidth <= 1.0f || imageHeight <= 1.0f) {
            return std::nullopt;
        }

        const float x = imageRectMin.x + (ndc.x * 0.5f + 0.5f) * imageWidth;
        const float y = imageRectMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * imageHeight;
        return ImVec2(x, y);
    }

    std::optional<ImVec2> projectViewPositionToImage(
        const glm::vec4& viewPosition,
        const glm::mat4& projectionMatrix,
        const ImVec2& imageRectMin,
        const ImVec2& imageRectMax
    ) {
        const glm::vec4 clipPosition = projectionMatrix * viewPosition;
        if (std::abs(clipPosition.w) <= 0.0001f) {
            return std::nullopt;
        }

        const glm::vec3 ndc = glm::vec3(clipPosition) / clipPosition.w;
        if (ndc.z < -2.0f || ndc.z > 2.0f) {
            return std::nullopt;
        }

        const float imageWidth = imageRectMax.x - imageRectMin.x;
        const float imageHeight = imageRectMax.y - imageRectMin.y;
        if (imageWidth <= 1.0f || imageHeight <= 1.0f) {
            return std::nullopt;
        }

        const float x = imageRectMin.x + (ndc.x * 0.5f + 0.5f) * imageWidth;
        const float y = imageRectMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * imageHeight;
        return ImVec2(x, y);
    }

    std::optional<std::pair<ImVec2, ImVec2>> projectWorldLineToImageClipped(
        const glm::vec3& startWorld,
        const glm::vec3& endWorld,
        const glm::mat4& viewMatrix,
        const glm::mat4& projectionMatrix,
        const ImVec2& imageRectMin,
        const ImVec2& imageRectMax,
        float nearClipDistance
    ) {
        glm::vec4 startView = viewMatrix * glm::vec4(startWorld, 1.0f);
        glm::vec4 endView = viewMatrix * glm::vec4(endWorld, 1.0f);

        const float nearPlaneViewZ = -std::max(0.01f, nearClipDistance);
        const bool startInFront = startView.z <= nearPlaneViewZ;
        const bool endInFront = endView.z <= nearPlaneViewZ;

        if (!startInFront && !endInFront) {
            return std::nullopt;
        }

        if (startInFront != endInFront) {
            const float deltaZ = endView.z - startView.z;
            if (std::abs(deltaZ) <= 0.00001f) {
                return std::nullopt;
            }

            float t = (nearPlaneViewZ - startView.z) / deltaZ;
            t = std::clamp(t, 0.0f, 1.0f);
            const glm::vec4 clippedView = startView + t * (endView - startView);
            if (!startInFront) {
                startView = clippedView;
            } else {
                endView = clippedView;
            }
        }

        const std::optional<ImVec2> startProjected = projectViewPositionToImage(startView, projectionMatrix, imageRectMin, imageRectMax);
        const std::optional<ImVec2> endProjected = projectViewPositionToImage(endView, projectionMatrix, imageRectMin, imageRectMax);
        if (!startProjected.has_value() || !endProjected.has_value()) {
            return std::nullopt;
        }

        return std::make_pair(startProjected.value(), endProjected.value());
    }

    ImGuizmo::OPERATION getGizmoOperationFromIndex(int operationIndex) {
        switch (operationIndex) {
            case 1:
                return ImGuizmo::ROTATE;
            case 2:
                return ImGuizmo::SCALE;
            case 0:
            default:
                return ImGuizmo::TRANSLATE;
        }
    }

    ImGuizmo::MODE getGizmoMode(bool worldMode) {
        return worldMode ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
    }

    float normalizeDegrees180(float degrees) {
        float normalized = std::fmod(degrees, 360.0f);
        if (normalized > 180.0f) {
            normalized -= 360.0f;
        } else if (normalized < -180.0f) {
            normalized += 360.0f;
        }

        return normalized;
    }

    float unwrapDegreesNear(float degrees, float referenceDegrees) {
        float unwrapped = normalizeDegrees180(degrees);
        const float reference = normalizeDegrees180(referenceDegrees);

        float delta = unwrapped - reference;
        if (delta > 180.0f) {
            unwrapped -= 360.0f;
        } else if (delta < -180.0f) {
            unwrapped += 360.0f;
        }

        return unwrapped;
    }

    glm::quat quaternionFromEulerDegreesXYZ(const glm::vec3& eulerDegrees) {
        const glm::quat qx = glm::angleAxis(glm::radians(eulerDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
        const glm::quat qy = glm::angleAxis(glm::radians(eulerDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::quat qz = glm::angleAxis(glm::radians(eulerDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
        return glm::normalize(qx * qy * qz);
    }

    glm::vec3 eulerDegreesFromQuaternionXYZ(const glm::quat& rotation, const glm::vec3& referenceDegrees) {
        const glm::mat4 rotationMatrix = glm::mat4_cast(glm::normalize(rotation));

        const float r00 = rotationMatrix[0][0];
        const float r01 = rotationMatrix[1][0];
        const float r02 = rotationMatrix[2][0];
        const float r10 = rotationMatrix[0][1];
        const float r11 = rotationMatrix[1][1];
        const float r12 = rotationMatrix[2][1];
        const float r22 = rotationMatrix[2][2];

        float xRadians = 0.0f;
        float yRadians = 0.0f;
        float zRadians = 0.0f;

        const float clampedR02 = std::clamp(r02, -1.0f, 1.0f);
        yRadians = std::asin(clampedR02);

        constexpr float kGimbalThreshold = 0.9999f;
        if (std::abs(clampedR02) < kGimbalThreshold) {
            xRadians = std::atan2(-r12, r22);
            zRadians = std::atan2(-r01, r00);
        } else {
            zRadians = 0.0f;
            if (clampedR02 > 0.0f) {
                xRadians = std::atan2(r10, r11);
            } else {
                xRadians = std::atan2(-r10, r11);
            }
        }

        glm::vec3 eulerDegrees = glm::degrees(glm::vec3(xRadians, yRadians, zRadians));
        eulerDegrees.x = unwrapDegreesNear(eulerDegrees.x, referenceDegrees.x);
        eulerDegrees.y = unwrapDegreesNear(eulerDegrees.y, referenceDegrees.y);
        eulerDegrees.z = unwrapDegreesNear(eulerDegrees.z, referenceDegrees.z);
        return eulerDegrees;
    }

    glm::mat3 extractOrthonormalRotationBasis(const glm::mat4& matrix) {
        constexpr float kEpsilon = 0.0001f;

        glm::vec3 xAxis(matrix[0]);
        glm::vec3 yAxis(matrix[1]);
        glm::vec3 zAxis(matrix[2]);

        if (glm::length(xAxis) < kEpsilon || glm::length(yAxis) < kEpsilon || glm::length(zAxis) < kEpsilon) {
            return glm::mat3(1.0f);
        }

        xAxis = glm::normalize(xAxis);

        yAxis = yAxis - xAxis * glm::dot(yAxis, xAxis);
        if (glm::length(yAxis) < kEpsilon) {
            yAxis = glm::cross(glm::normalize(zAxis), xAxis);
        }
        if (glm::length(yAxis) < kEpsilon) {
            yAxis = glm::vec3(0.0f, 1.0f, 0.0f);
        } else {
            yAxis = glm::normalize(yAxis);
        }

        zAxis = glm::cross(xAxis, yAxis);
        if (glm::length(zAxis) < kEpsilon) {
            zAxis = glm::vec3(matrix[2]);
        }
        if (glm::length(zAxis) < kEpsilon) {
            zAxis = glm::vec3(0.0f, 0.0f, 1.0f);
        } else {
            zAxis = glm::normalize(zAxis);
        }

        if (glm::dot(zAxis, glm::vec3(matrix[2])) < 0.0f) {
            zAxis = -zAxis;
            yAxis = -yAxis;
        }

        return glm::mat3(xAxis, yAxis, zAxis);
    }

    void applyMatrixToSceneTransform(const glm::mat4& matrix, SceneTransform& transform, ImGuizmo::OPERATION operation, float rotationSensitivity) {
        float translation[3] = {0.0f, 0.0f, 0.0f};
        float rotation[3] = {0.0f, 0.0f, 0.0f};
        float scale[3] = {1.0f, 1.0f, 1.0f};
        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(matrix), translation, rotation, scale);

        const glm::vec3 sizeBasis(
            std::max(0.01f, std::abs(transform.size.x)),
            std::max(0.01f, std::abs(transform.size.y)),
            std::max(0.01f, std::abs(transform.size.z))
        );

        transform.position = glm::vec3(translation[0], translation[1], translation[2]);

        const bool rotateOperation = operation == ImGuizmo::ROTATE;
        if (rotateOperation) {
            const glm::quat currentRotation = quaternionFromEulerDegreesXYZ(transform.rotation);
            glm::quat targetRotation = glm::normalize(glm::quat_cast(extractOrthonormalRotationBasis(matrix)));

            const float clampedSensitivity = std::clamp(rotationSensitivity, 0.05f, 2.0f);
            if (std::abs(clampedSensitivity - 1.0f) > 0.001f) {
                glm::quat deltaRotation = glm::normalize(targetRotation * glm::inverse(currentRotation));
                float deltaAngleRadians = 2.0f * std::acos(std::clamp(deltaRotation.w, -1.0f, 1.0f));

                glm::vec3 deltaAxis(deltaRotation.x, deltaRotation.y, deltaRotation.z);
                const float axisLength = glm::length(deltaAxis);
                if (axisLength > 0.0001f) {
                    deltaAxis /= axisLength;
                } else {
                    deltaAxis = glm::vec3(0.0f, 1.0f, 0.0f);
                }

                if (deltaAngleRadians > glm::pi<float>()) {
                    deltaAngleRadians -= glm::two_pi<float>();
                }

                const float scaledAngleRadians = deltaAngleRadians * clampedSensitivity;
                targetRotation = glm::normalize(glm::angleAxis(scaledAngleRadians, deltaAxis) * currentRotation);
            }

            transform.rotation = eulerDegreesFromQuaternionXYZ(targetRotation, transform.rotation);
        }

        const int operationMask = static_cast<int>(operation);
        const bool scaleOperation =
            (operationMask & static_cast<int>(ImGuizmo::SCALE_X)) != 0 ||
            (operationMask & static_cast<int>(ImGuizmo::SCALE_Y)) != 0 ||
            (operationMask & static_cast<int>(ImGuizmo::SCALE_Z)) != 0 ||
            (operationMask & static_cast<int>(ImGuizmo::SCALE_XU)) != 0 ||
            (operationMask & static_cast<int>(ImGuizmo::SCALE_YU)) != 0 ||
            (operationMask & static_cast<int>(ImGuizmo::SCALE_ZU)) != 0;

        if (scaleOperation) {
            transform.scale = glm::vec3(
                std::max(0.01f, std::abs(scale[0]) / sizeBasis.x),
                std::max(0.01f, std::abs(scale[1]) / sizeBasis.y),
                std::max(0.01f, std::abs(scale[2]) / sizeBasis.z)
            );
        }
    }

    void drawWindowPanelGradient() {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (drawList == nullptr) {
            return;
        }

        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 windowSize = ImGui::GetWindowSize();
        const ImVec2 windowMax(windowPos.x + windowSize.x, windowPos.y + windowSize.y);

        drawList->AddRectFilledMultiColor(
            windowPos,
            windowMax,
            IM_COL32(28, 30, 32, 150),
            IM_COL32(28, 30, 32, 150),
            IM_COL32(17, 18, 20, 185),
            IM_COL32(17, 18, 20, 185)
        );
        drawList->AddRect(windowPos, windowMax, IM_COL32(142, 38, 38, 140), 0.0f, 0, 1.0f);
    }

    void drawHorizontalSceneGrid(
        ImDrawList* drawList,
        const glm::mat4& viewMatrix,
        const glm::mat4& projectionMatrix,
        const ImVec2& imageRectMin,
        const ImVec2& imageRectMax,
        int halfExtent,
        float spacing
    ) {
        if (drawList == nullptr) {
            return;
        }

        const int clampedHalfExtent = std::clamp(halfExtent, 1, 200);
        const float clampedSpacing = std::clamp(spacing, 0.1f, 100.0f);
        const float extent = static_cast<float>(clampedHalfExtent) * clampedSpacing;
        const float nearClipDistance = 0.02f;

        const ImU32 xLineColor = IM_COL32(180, 80, 80, 95);
        const ImU32 zLineColor = IM_COL32(80, 120, 180, 95);
        const ImU32 xAxisColor = IM_COL32(240, 90, 90, 235);
        const ImU32 yAxisColor = IM_COL32(95, 225, 120, 235);
        const ImU32 zAxisColor = IM_COL32(95, 145, 240, 235);

        for (int lineIndex = -clampedHalfExtent; lineIndex <= clampedHalfExtent; ++lineIndex) {
            const float x = static_cast<float>(lineIndex) * clampedSpacing;
            const std::optional<std::pair<ImVec2, ImVec2>> projectedLine = projectWorldLineToImageClipped(
                glm::vec3(x, 0.0f, -extent),
                glm::vec3(x, 0.0f, extent),
                viewMatrix,
                projectionMatrix,
                imageRectMin,
                imageRectMax,
                nearClipDistance
            );

            if (projectedLine.has_value()) {
                const ImU32 lineColor = (lineIndex == 0) ? zAxisColor : xLineColor;
                const float thickness = (lineIndex == 0) ? 2.2f : 1.0f;
                drawList->AddLine(projectedLine->first, projectedLine->second, lineColor, thickness);
            }
        }

        for (int lineIndex = -clampedHalfExtent; lineIndex <= clampedHalfExtent; ++lineIndex) {
            const float z = static_cast<float>(lineIndex) * clampedSpacing;
            const std::optional<std::pair<ImVec2, ImVec2>> projectedLine = projectWorldLineToImageClipped(
                glm::vec3(-extent, 0.0f, z),
                glm::vec3(extent, 0.0f, z),
                viewMatrix,
                projectionMatrix,
                imageRectMin,
                imageRectMax,
                nearClipDistance
            );

            if (projectedLine.has_value()) {
                const ImU32 lineColor = (lineIndex == 0) ? xAxisColor : zLineColor;
                const float thickness = (lineIndex == 0) ? 2.2f : 1.0f;
                drawList->AddLine(projectedLine->first, projectedLine->second, lineColor, thickness);
            }
        }

        const float axisLength = std::max(2.0f, extent * 0.35f);
        const std::optional<std::pair<ImVec2, ImVec2>> xAxis = projectWorldLineToImageClipped(
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(axisLength, 0.0f, 0.0f),
            viewMatrix,
            projectionMatrix,
            imageRectMin,
            imageRectMax,
            nearClipDistance
        );
        const std::optional<std::pair<ImVec2, ImVec2>> yAxis = projectWorldLineToImageClipped(
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, axisLength, 0.0f),
            viewMatrix,
            projectionMatrix,
            imageRectMin,
            imageRectMax,
            nearClipDistance
        );
        const std::optional<std::pair<ImVec2, ImVec2>> zAxis = projectWorldLineToImageClipped(
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, axisLength),
            viewMatrix,
            projectionMatrix,
            imageRectMin,
            imageRectMax,
            nearClipDistance
        );

        if (xAxis.has_value()) {
            drawList->AddLine(xAxis->first, xAxis->second, xAxisColor, 2.4f);
            drawList->AddText(ImVec2(xAxis->second.x + 4.0f, xAxis->second.y), xAxisColor, "X");
        }
        if (yAxis.has_value()) {
            drawList->AddLine(yAxis->first, yAxis->second, yAxisColor, 2.4f);
            drawList->AddText(ImVec2(yAxis->second.x + 4.0f, yAxis->second.y), yAxisColor, "Y");
        }
        if (zAxis.has_value()) {
            drawList->AddLine(zAxis->first, zAxis->second, zAxisColor, 2.4f);
            drawList->AddText(ImVec2(zAxis->second.x + 4.0f, zAxis->second.y), zAxisColor, "Z");
        }
    }

    bool drawColoredAxisFloatControl(const char* id, const char* axisLabel, const ImVec4& color, float* value, float speed, bool hasLimits, float minValue, float maxValue) {
        bool changed = false;
        ImGui::PushID(id);

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(axisLabel);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(92.0f);
        changed = ImGui::DragFloat("##value", value, speed, hasLimits ? minValue : 0.0f, hasLimits ? maxValue : 0.0f, "%.3f");

        ImGui::PopID();
        return changed;
    }

    bool drawColoredVec3Control(const char* label, glm::vec3& value, float speed, bool hasLimits = false, float minValue = 0.0f, float maxValue = 0.0f) {
        bool changed = false;
        ImGui::TextUnformatted(label);
        ImGui::SameLine();

        changed |= drawColoredAxisFloatControl((std::string(label) + "_X").c_str(), "X", ImVec4(0.95f, 0.38f, 0.38f, 1.0f), &value.x, speed, hasLimits, minValue, maxValue);
        ImGui::SameLine();
        changed |= drawColoredAxisFloatControl((std::string(label) + "_Y").c_str(), "Y", ImVec4(0.36f, 0.87f, 0.44f, 1.0f), &value.y, speed, hasLimits, minValue, maxValue);
        ImGui::SameLine();
        changed |= drawColoredAxisFloatControl((std::string(label) + "_Z").c_str(), "Z", ImVec4(0.42f, 0.63f, 0.95f, 1.0f), &value.z, speed, hasLimits, minValue, maxValue);

        return changed;
    }

    std::optional<float> rayAabbIntersectionDistance(
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection,
        const glm::vec3& boundsMin,
        const glm::vec3& boundsMax
    ) {
        constexpr float kParallelEpsilon = 0.000001f;

        float tMin = 0.0f;
        float tMax = std::numeric_limits<float>::max();

        auto testAxis = [&](float origin, float direction, float minBound, float maxBound) {
            if (std::abs(direction) < kParallelEpsilon) {
                return origin >= minBound && origin <= maxBound;
            }

            const float inverseDirection = 1.0f / direction;
            float t0 = (minBound - origin) * inverseDirection;
            float t1 = (maxBound - origin) * inverseDirection;
            if (t0 > t1) {
                std::swap(t0, t1);
            }

            tMin = std::max(tMin, t0);
            tMax = std::min(tMax, t1);
            return tMax >= tMin;
        };

        if (!testAxis(rayOrigin.x, rayDirection.x, boundsMin.x, boundsMax.x)) {
            return std::nullopt;
        }
        if (!testAxis(rayOrigin.y, rayDirection.y, boundsMin.y, boundsMax.y)) {
            return std::nullopt;
        }
        if (!testAxis(rayOrigin.z, rayDirection.z, boundsMin.z, boundsMax.z)) {
            return std::nullopt;
        }

        if (tMax < 0.0f) {
            return std::nullopt;
        }

        return tMin >= 0.0f ? tMin : tMax;
    }

    std::optional<float> raySphereIntersectionDistance(
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection,
        const glm::vec3& sphereCenter,
        float sphereRadius
    ) {
        const glm::vec3 oc = rayOrigin - sphereCenter;
        const float b = 2.0f * glm::dot(oc, rayDirection);
        const float c = glm::dot(oc, oc) - (sphereRadius * sphereRadius);
        const float discriminant = b * b - 4.0f * c;
        if (discriminant < 0.0f) {
            return std::nullopt;
        }

        const float root = std::sqrt(discriminant);
        const float nearDistance = (-b - root) * 0.5f;
        const float farDistance = (-b + root) * 0.5f;
        if (nearDistance > 0.0f) {
            return nearDistance;
        }

        if (farDistance > 0.0f) {
            return farDistance;
        }

        return std::nullopt;
    }

    void UILayer::bindEngineSettings(Window* window, EngineConfig* engineConfig) {
        m_window = window;
        m_engineConfig = engineConfig;
        syncEngineSettingsEditorState();
    }

    void UILayer::setSelectedEntity(int entityIndex) {
        const auto& entities = m_activeScene.getEntityData();
        if (entityIndex < 0 || entityIndex >= static_cast<int>(entities.size())) {
            clearEntitySelection();
            return;
        }

        m_selectedEntityIndex = entityIndex;
        m_selectedEntityNameBufferEntityIndex = -1;
        m_activeScene.setGameStateValue("SelectedEntity", entities[static_cast<std::size_t>(entityIndex)].name);
    }

    void UILayer::clearEntitySelection() {
        m_selectedEntityIndex = -1;
        m_selectedEntityNameBufferEntityIndex = -1;
        m_activeScene.setGameStateValue("SelectedEntity", "None");
    }

    void UILayer::syncActiveSceneToLibrary() {
        if (m_activeSceneLibraryIndex < 0) {
            return;
        }

        const std::size_t sceneIndex = static_cast<std::size_t>(m_activeSceneLibraryIndex);
        if (sceneIndex >= m_editorScenes.size()) {
            return;
        }

        m_editorScenes[sceneIndex] = m_activeScene;
    }

    bool UILayer::switchToSceneByIndex(std::size_t sceneIndex) {
        if (sceneIndex >= m_editorScenes.size()) {
            return false;
        }

        syncActiveSceneToLibrary();

        m_activeSceneLibraryIndex = static_cast<int>(sceneIndex);
        m_activeScene = m_editorScenes[sceneIndex];
        clearEntitySelection();
        m_sceneCameraControllerInitialized = false;

        appendTerminalLine("Switched to scene " + m_activeScene.getName() + ".");
        return true;
    }

    void UILayer::openAssetImportBrowser(RuntimeImportMode mode, const char* title) {
        m_assetImportMode = mode;
        m_assetImportDialogTitle = title != nullptr ? title : "Import Asset";
        m_assetImportSelectedPath.clear();

        std::error_code ec;
        const std::filesystem::path defaultDir = FileSystem::getAssetRootDirectory();
        if (!defaultDir.empty() && std::filesystem::exists(defaultDir, ec)) {
            m_assetImportCurrentDirectory = defaultDir;
        } else {
            m_assetImportCurrentDirectory = std::filesystem::current_path(ec);
        }

        m_assetImportBrowserOpen = true;
        m_assetImportBrowserRequestOpen = true;
    }

    bool UILayer::isAssetPathAllowedForMode(const std::filesystem::path& path, RuntimeImportMode mode) const {
        return RuntimeAssetImportService::isPathAllowedForMode(path, mode);
    }

    void UILayer::drawAssetImportFileBrowser() {
        if (!m_assetImportBrowserOpen) {
            return;
        }

        VALKRON_CORE_ASSERT(!m_assetImportDialogTitle.empty(), "Asset import dialog title must not be empty");

        if (m_assetImportBrowserRequestOpen) {
            ImGui::OpenPopup(m_assetImportDialogTitle.c_str());
            m_assetImportBrowserRequestOpen = false;
        }

        bool keepOpen = m_assetImportBrowserOpen;
        ImGui::SetNextWindowSize(ImVec2(760.0f, 460.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::BeginPopupModal(m_assetImportDialogTitle.c_str(), &keepOpen, ImGuiWindowFlags_NoCollapse)) {
            m_assetImportBrowserOpen = keepOpen;
            return;
        }

        std::error_code ec;
        if (m_assetImportCurrentDirectory.empty()) {
            m_assetImportCurrentDirectory = std::filesystem::current_path(ec);
        }

        ImGui::Text("Current Directory");
        ImGui::TextWrapped("%s", m_assetImportCurrentDirectory.string().c_str());

        if (ImGui::Button("Up")) {
            const std::filesystem::path parent = m_assetImportCurrentDirectory.parent_path();
            if (!parent.empty()) {
                m_assetImportCurrentDirectory = parent;
                m_assetImportSelectedPath.clear();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Assets Root")) {
            const std::filesystem::path assetRoot = FileSystem::getAssetRootDirectory();
            if (!assetRoot.empty() && std::filesystem::exists(assetRoot, ec)) {
                m_assetImportCurrentDirectory = assetRoot;
                m_assetImportSelectedPath.clear();
            }
        }

        std::vector<std::filesystem::path> directories;
        std::vector<std::filesystem::path> files;
        if (std::filesystem::exists(m_assetImportCurrentDirectory, ec)) {
            for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(m_assetImportCurrentDirectory, ec)) {
                if (entry.is_directory(ec)) {
                    directories.push_back(entry.path());
                } else if (entry.is_regular_file(ec) && isAssetPathAllowedForMode(entry.path(), m_assetImportMode)) {
                    files.push_back(entry.path());
                }
            }
        }

        std::sort(directories.begin(), directories.end());
        std::sort(files.begin(), files.end());

        if (ImGui::BeginChild("AssetImportBrowserEntries", ImVec2(0.0f, -56.0f), true)) {
            for (const std::filesystem::path& dirPath : directories) {
                const std::string label = "[DIR] " + dirPath.filename().string();
                if (ImGui::Selectable(label.c_str(), false)) {
                    m_assetImportCurrentDirectory = dirPath;
                    m_assetImportSelectedPath.clear();
                }
            }

            for (const std::filesystem::path& filePath : files) {
                const bool selected = filePath == m_assetImportSelectedPath;
                if (ImGui::Selectable(filePath.filename().string().c_str(), selected)) {
                    m_assetImportSelectedPath = filePath;
                }
            }
        }
        ImGui::EndChild();

        ImGui::TextWrapped("Selected: %s", m_assetImportSelectedPath.empty() ? "None" : m_assetImportSelectedPath.string().c_str());

        const bool hasSelection = !m_assetImportSelectedPath.empty();
        if (!hasSelection) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Import", ImVec2(120.0f, 0.0f))) {
            const bool imported = importRuntimeAssetFromPath(m_assetImportSelectedPath.string(), m_assetImportMode);
            if (imported) {
                AssetManager::saveSceneAssetCache(m_activeScene);
                m_assetImportBrowserOpen = false;
                ImGui::CloseCurrentPopup();
            }
        }
        if (!hasSelection) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
            m_assetImportBrowserOpen = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();

        m_assetImportBrowserOpen = keepOpen && m_assetImportBrowserOpen;
    }

    bool UILayer::importRuntimeAssetFromPath(const std::string& assetPath, RuntimeImportMode mode) {
        const RuntimeAssetImportResult result = RuntimeAssetImportService::importAsset(
            m_activeScene,
            assetPath,
            mode,
            std::max(1, m_runtimeTexture3DDepth)
        );

        for (const std::string& message : result.messages) {
            appendTerminalLine(message);
        }

        return result.success;
    }

    void UILayer::onAttach() {
        auto buildEditorScene = [](const std::string& sceneName, const std::string& subjectName, const glm::vec3& subjectPosition, const glm::vec3& subjectScale) {
            Scene scene(sceneName);

            scene.addEntity("Camera", SceneEntityType::Camera);
            scene.addEntity("Directional Light", SceneEntityType::Light);
            scene.addEntity(subjectName, SceneEntityType::Generic);

            if (const std::optional<std::size_t> cameraEntityIndex = scene.findEntityIndex("Camera"); cameraEntityIndex.has_value()) {
                if (SceneEntity* cameraEntity = scene.getEntityByIndex(cameraEntityIndex.value()); cameraEntity != nullptr) {
                    cameraEntity->transform.position = glm::vec3(0.0f, 1.6f, 4.0f);
                    cameraEntity->transform.rotation = glm::vec3(-12.0f, 0.0f, 0.0f);
                }
            }

            if (const std::optional<std::size_t> lightEntityIndex = scene.findEntityIndex("Directional Light"); lightEntityIndex.has_value()) {
                if (SceneEntity* lightEntity = scene.getEntityByIndex(lightEntityIndex.value()); lightEntity != nullptr) {
                    lightEntity->transform.position = glm::vec3(3.0f, 4.0f, 3.0f);
                }
            }

            if (const std::optional<std::size_t> subjectEntityIndex = scene.findEntityIndex(subjectName); subjectEntityIndex.has_value()) {
                if (SceneEntity* subjectEntity = scene.getEntityByIndex(subjectEntityIndex.value()); subjectEntity != nullptr) {
                    subjectEntity->transform.position = subjectPosition;
                    subjectEntity->transform.scale = subjectScale;
                    subjectEntity->transform.size = glm::vec3(1.0f, 1.0f, 1.0f);
                    subjectEntity->modelAssetName = "test_cube.obj";
                }
            }

            scene.addAsset("checker.ppm", "assets/textures/checker.ppm");
            scene.addAsset("textured.vert", "assets/shaders/textured.vert");
            scene.addAsset("textured.frag", "assets/shaders/textured.frag");
            scene.addAsset("blinn_phong.vert", "assets/shaders/blinn_phong.vert");
            scene.addAsset("blinn_phong.frag", "assets/shaders/blinn_phong.frag");
            scene.addAsset("default_compute.comp", "assets/shaders/default_compute.comp");
            scene.addAsset("test_cube.obj", "assets/models/test_cube.obj");

            scene.addScript("SpinController", "scripts/SpinController.lua", true);
            scene.addScript("CameraOrbit", "scripts/CameraOrbit.lua", false);

            scene.addCamera("Editor Camera", CameraType::Perspective, true);
            scene.addCamera("Ortho Debug Camera", CameraType::Orthographic, false);

            scene.setSetting("Renderer.VSync", "true");
            scene.setSetting("Renderer.MSAA", "4");
            scene.setSetting("Physics.Gravity", "0,-9.81,0");
            scene.setGameStateValue("Mode", "Edit");
            scene.setGameStateValue("SelectedEntity", "None");
            scene.setGameStateValue("PrimaryCameraEntity", "Camera");

            return scene;
        };

        m_editorScenes.clear();
        m_editorScenes.push_back(buildEditorScene("Sandbox Scene", "Cube", glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f)));
        m_editorScenes.push_back(buildEditorScene("Lighting Study", "Statue", glm::vec3(-1.4f, 0.0f, 0.8f), glm::vec3(1.35f, 1.35f, 1.35f)));
        m_editorScenes.push_back(buildEditorScene("Gameplay Blockout", "Crate", glm::vec3(1.7f, 0.0f, -1.2f), glm::vec3(0.85f, 0.85f, 0.85f)));

        m_activeSceneLibraryIndex = 0;
        m_activeScene = m_editorScenes[static_cast<std::size_t>(m_activeSceneLibraryIndex)];

        AssetLoader::initialize();
        AssetManager::setCachePath(AssetManager::getDefaultCachePath());
        AssetManager::loadSceneAssetCache(m_activeScene);

        auto loadUiIconTexture = [](const std::string& iconPath) -> std::shared_ptr<Texture> {
            auto texture = std::make_shared<Texture>();
            if (!texture->loadTexture(iconPath, false)) {
                return nullptr;
            }

            return texture;
        };

        m_cameraEntityIconTexture = loadUiIconTexture("assets/icons/camera.png");
        m_lightEntityIconTexture = loadUiIconTexture("assets/icons/light.png");
        m_assetDirectoryIconTexture = loadUiIconTexture("assets/icons/directory.png");
        m_assetImportIconTexture = loadUiIconTexture("assets/icons/import.png");

        const bool hasBlinnShaderAsset = std::any_of(
            m_activeScene.getAssets().begin(),
            m_activeScene.getAssets().end(),
            [](const SceneAsset& asset) { return asset.name == "blinn_phong.vert"; }
        );

        if (!hasBlinnShaderAsset) {
            m_activeScene.addAsset("blinn_phong.vert", "assets/shaders/blinn_phong.vert");
            m_activeScene.addAsset("blinn_phong.frag", "assets/shaders/blinn_phong.frag");
            m_activeScene.addAsset("default_compute.comp", "assets/shaders/default_compute.comp");
            m_activeScene.addAsset("test_cube.obj", "assets/models/test_cube.obj");
        }

        m_terminalLines.clear();
        m_selectedEntityIndex = -1;
        m_selectedEntityNameBufferEntityIndex = -1;
        std::fill(m_selectedEntityNameBuffer.begin(), m_selectedEntityNameBuffer.end(), '\0');
        std::fill(m_hierarchySearchBuffer.begin(), m_hierarchySearchBuffer.end(), '\0');
        m_pendingViewportWidth = 0;
        m_pendingViewportHeight = 0;
        m_viewportResizeDebounceTimer = 0.0f;
        m_sceneViewImageHovered = false;
        m_showSceneGrid = true;
        m_sceneGridHalfExtent = 12;
        m_sceneGridSpacing = 1.0f;
        m_sceneCameraControllerInitialized = false;
        m_runtimeTexture3DDepth = std::max(1, m_runtimeTexture3DDepth);
        m_assetBrowserFolderFilter.clear();
        m_selectedAssetIndex = -1;
        m_assetBrowserNavigationHistory.clear();
        m_assetBrowserNavigationHistory.push_back(std::string{});
        m_assetBrowserNavigationIndex = 0;
        m_assetImportBrowserOpen = false;
        m_assetImportBrowserRequestOpen = false;
        m_assetImportMode = RuntimeImportMode::Auto;
        m_assetImportDialogTitle = "Import Asset";
        m_assetImportCurrentDirectory.clear();
        m_assetImportSelectedPath.clear();
        m_showSettingsPanel = false;
        m_showDebugPanel = true;
        m_dockspaceBuilt = false;
        m_gizmoOperationIndex = 0;
        m_gizmoWorldMode = false;
        m_showGameViewPanel = true;

        m_topNavbarPanelController = std::make_unique<TopNavbarPanel>([this]() {
            drawTopNavbar();
        });
        m_dockspacePanelController = std::make_unique<DockspacePanel>([this]() {
            drawDockspaceHost();
        });
        m_sceneHierarchyPanelController = std::make_unique<SceneHierarchyPanel>([this]() {
            drawSceneHierarchyPanel();
        });
        m_sceneViewPanelController = std::make_unique<SceneViewPanel>([this](float frameDeltaTime) {
            drawSceneViewPanel(frameDeltaTime);
        });
        m_gameViewPanelController = std::make_unique<GameViewPanel>([this](float frameDeltaTime) {
            drawGameViewPanel(frameDeltaTime);
        });
        m_inspectorPanelController = std::make_unique<InspectorPanel>([this]() {
            drawInspectorPanel();
        });
        m_settingsPanelController = std::make_unique<SettingsPanel>([this]() {
            drawSettingsPanel();
        });
        m_debugPanelController = std::make_unique<DebugPanel>([this]() {
            drawDebugPanel();
        });
        m_assetBrowserPanelController = std::make_unique<AssetBrowserPanel>([this]() {
            drawBottomPanel();
        });

        syncEngineSettingsEditorState();
        appendTerminalLine("UI Manager attached (Dear ImGui). Editor layout ready.");
    }

    void UILayer::onDetach() {
        m_topNavbarPanelController.reset();
        m_dockspacePanelController.reset();
        m_sceneHierarchyPanelController.reset();
        m_sceneViewPanelController.reset();
        m_gameViewPanelController.reset();
        m_inspectorPanelController.reset();
        m_settingsPanelController.reset();
        m_debugPanelController.reset();
        m_assetBrowserPanelController.reset();

        m_cameraEntityIconTexture.reset();
        m_lightEntityIconTexture.reset();
        m_assetDirectoryIconTexture.reset();
        m_assetImportIconTexture.reset();
        appendTerminalLine("UI Manager detached.");
        LOG_DEBUG("UILayer detached.");
    }

    void UILayer::onUpdate(float deltaTime) {
        if (m_topNavbarPanelController != nullptr) {
            m_topNavbarPanelController->render(deltaTime);
        }

        if (m_dockspacePanelController != nullptr) {
            m_dockspacePanelController->render(deltaTime);
        }

        ImGuizmo::BeginFrame();

        m_lastFrameDeltaTimeSeconds = std::max(0.0f, deltaTime);

        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        if (displaySize.x <= 0.0f || displaySize.y <= 0.0f) {
            return;
        }

        const std::vector<SceneEntity>& entities = m_activeScene.getEntityData();
        if (m_selectedEntityIndex >= static_cast<int>(entities.size())) {
            clearEntitySelection();
        }

        std::vector<SceneModelInstance> sceneModelInstances;
        std::vector<glm::vec3> lightEntityPositions;
        sceneModelInstances.reserve(entities.size());
        lightEntityPositions.reserve(entities.size());

        const std::optional<std::size_t> primaryPlayCameraEntityIndex = resolvePrimaryPlayCameraEntityIndex(m_activeScene);
        if (primaryPlayCameraEntityIndex.has_value() && primaryPlayCameraEntityIndex.value() < entities.size()) {
            const std::string& resolvedCameraName = entities[primaryPlayCameraEntityIndex.value()].name;
            if (m_activeScene.getGameStateValue("PrimaryCameraEntity").value_or("") != resolvedCameraName) {
                m_activeScene.setGameStateValue("PrimaryCameraEntity", resolvedCameraName);
            }
        }

        std::optional<glm::mat4> runtimeCameraTransform;
        std::optional<glm::vec3> directionalLightDirection;
        for (std::size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex) {
            const glm::mat4 worldTransform = composeEntityWorldTransformMatrix(entities, entityIndex);
            const bool cameraEntity = isCameraEntity(entities[entityIndex]);
            const bool lightEntity = isLightEntity(entities[entityIndex]);

            if (lightEntity) {
                const glm::vec3 lightPosition = extractWorldPosition(worldTransform);
                lightEntityPositions.push_back(lightPosition);

                if (!directionalLightDirection.has_value()) {
                    glm::vec3 candidateDirection = extractForwardDirection(worldTransform);
                    const SceneTransform& lightTransform = entities[entityIndex].transform;
                    const bool hasExplicitRotation = glm::length(lightTransform.rotation) > 0.001f;
                    if (!hasExplicitRotation && glm::length(lightPosition) > 0.001f) {
                        candidateDirection = glm::normalize(-lightPosition);
                    }

                    directionalLightDirection = candidateDirection;
                }
            }

            if (cameraEntity && primaryPlayCameraEntityIndex.has_value() && entityIndex == primaryPlayCameraEntityIndex.value()) {
                runtimeCameraTransform = worldTransform;
            }

            if (!cameraEntity && !lightEntity && !entities[entityIndex].modelAssetName.empty()) {
                sceneModelInstances.push_back(SceneModelInstance{
                    entities[entityIndex].modelAssetName,
                    worldTransform,
                    static_cast<int>(entityIndex) == m_selectedEntityIndex
                });
            }
        }

        Renderer::setSceneModelInstances(sceneModelInstances);
        Renderer::setLightEntityPositions(lightEntityPositions);

        if (directionalLightDirection.has_value()) {
            Renderer::setDirectionalLight(directionalLightDirection.value(), glm::vec3(1.0f, 0.98f, 0.95f), 1.15f, 0.24f);
        } else {
            Renderer::setDirectionalLight(glm::vec3(-0.40f, -1.00f, -0.30f), glm::vec3(1.0f, 0.98f, 0.95f), 1.05f, 0.22f);
        }

        m_runtimeEntityCameraActive = false;
        if (m_activeScene.getState() == SceneState::Play && runtimeCameraTransform.has_value()) {
            const glm::vec3 cameraPosition = extractWorldPosition(runtimeCameraTransform.value());
            const glm::vec3 cameraForward = extractForwardDirection(runtimeCameraTransform.value());
            Renderer::setCameraLookAt(cameraPosition, cameraPosition + cameraForward);
            m_runtimeEntityCameraActive = true;
        }

        if (m_showSceneHierarchyPanel) {
            if (m_sceneHierarchyPanelController != nullptr) {
                m_sceneHierarchyPanelController->render(deltaTime);
            }
        }

        if (m_showSceneViewPanel) {
            if (m_sceneViewPanelController != nullptr) {
                m_sceneViewPanelController->render(deltaTime);
            }
        }

        if (m_showGameViewPanel) {
            if (m_gameViewPanelController != nullptr) {
                m_gameViewPanelController->render(deltaTime);
            }
        }

        if (m_showInspectorPanel) {
            if (m_inspectorPanelController != nullptr) {
                m_inspectorPanelController->render(deltaTime);
            }
        }

        if (m_showSettingsPanel) {
            if (m_settingsPanelController != nullptr) {
                m_settingsPanelController->render(deltaTime);
            }
        }

        if (m_showAssetManagerPanel || m_showConsolePanel) {
            if (m_assetBrowserPanelController != nullptr) {
                m_assetBrowserPanelController->render(deltaTime);
            }
        }

        if (m_showDebugPanel) {
            if (m_debugPanelController != nullptr) {
                m_debugPanelController->render(deltaTime);
            }
        }

        drawAssetImportFileBrowser();

        syncActiveSceneToLibrary();

        m_resetLayoutRequested = false;
    }

    void UILayer::onEvent(Event& event) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse && (
            event.type == EventType::MouseButton ||
            event.type == EventType::MouseMove ||
            event.type == EventType::MouseScroll
        )) {
            event.handled = true;
            return;
        }

        if (io.WantCaptureKeyboard && event.type == EventType::Key) {
            event.handled = true;
        }
    }

    void UILayer::initializeSceneCameraController() {
        if (m_sceneCameraControllerInitialized) {
            return;
        }

        const glm::vec3 cameraPosition = Renderer::getCameraPosition();
        const glm::vec3 cameraTarget = Renderer::getCameraTarget();
        m_sceneCameraUp = Renderer::getCameraUp();
        m_sceneCameraPivot = cameraTarget;

        glm::vec3 orbitOffset = cameraPosition - cameraTarget;
        float orbitDistance = glm::length(orbitOffset);
        if (orbitDistance < 0.01f) {
            orbitOffset = glm::vec3(0.0f, 0.0f, 2.0f);
            orbitDistance = 2.0f;
        }

        m_sceneCameraDistance = std::max(0.15f, orbitDistance);
        m_sceneCameraPitchRadians = std::clamp(std::asin(orbitOffset.y / m_sceneCameraDistance), -1.52f, 1.52f);
        m_sceneCameraYawRadians = std::atan2(orbitOffset.x, orbitOffset.z);
        m_sceneCameraControllerInitialized = true;
    }

    void UILayer::syncRendererCameraFromController() {
        const glm::vec3 offset = computeOrbitOffset(m_sceneCameraDistance, m_sceneCameraYawRadians, m_sceneCameraPitchRadians);
        const glm::vec3 cameraPosition = m_sceneCameraPivot + offset;
        Renderer::setCameraLookAt(cameraPosition, m_sceneCameraPivot, m_sceneCameraUp);
    }

    void UILayer::updateSceneCameraController(bool sceneImageHovered) {
        initializeSceneCameraController();

        ImGuiIO& io = ImGui::GetIO();
        bool changedCamera = false;

        if (sceneImageHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            m_sceneCameraYawRadians -= io.MouseDelta.x * m_sceneCameraRotateSpeed;
            m_sceneCameraPitchRadians -= io.MouseDelta.y * m_sceneCameraRotateSpeed;
            m_sceneCameraPitchRadians = std::clamp(m_sceneCameraPitchRadians, -1.52f, 1.52f);
            changedCamera = true;
        }

        if (sceneImageHovered && io.KeyCtrl && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            const glm::vec3 orbitUnitOffset = computeOrbitOffset(1.0f, m_sceneCameraYawRadians, m_sceneCameraPitchRadians);
            const glm::vec3 forward = glm::normalize(-orbitUnitOffset);
            const glm::vec3 right = glm::normalize(glm::cross(forward, m_sceneCameraUp));
            const glm::vec3 viewUp = glm::normalize(glm::cross(right, forward));

            glm::vec3 panOffset = ((right * -io.MouseDelta.x) + (viewUp * io.MouseDelta.y)) * (m_sceneCameraDistance * m_sceneCameraPanSpeed);
            if (m_sceneCameraInvertPan) {
                panOffset *= -1.0f;
            }

            m_sceneCameraPivot += panOffset;
            changedCamera = true;
        }

        if (sceneImageHovered && std::abs(io.MouseWheel) > 0.0001f) {
            m_sceneCameraDistance *= std::exp(-io.MouseWheel * m_sceneCameraZoomSpeed);
            m_sceneCameraDistance = std::clamp(m_sceneCameraDistance, 0.15f, 500.0f);
            changedCamera = true;
        }

        if (changedCamera) {
            syncRendererCameraFromController();
        }
    }

    bool UILayer::loadRuntimeTexture2D() {
        openAssetImportBrowser(RuntimeImportMode::Texture2D, "Import 2D Texture");
        return true;
    }

    bool UILayer::loadRuntimeTexture3D() {
        m_runtimeTexture3DDepth = std::max(1, m_runtimeTexture3DDepth);
        openAssetImportBrowser(RuntimeImportMode::Texture3D, "Import 3D Texture");
        return true;
    }

    bool UILayer::loadRuntimeShader() {
        openAssetImportBrowser(RuntimeImportMode::Shader, "Import Shader Pair");
        return true;
    }

    bool UILayer::loadRuntimeComputeShader() {
        openAssetImportBrowser(RuntimeImportMode::Compute, "Import Compute Shader");
        return true;
    }

    bool UILayer::loadRuntimeModel() {
        openAssetImportBrowser(RuntimeImportMode::Model, "Import Model");
        return true;
    }

    bool UILayer::loadRuntimeAssetAuto() {
        openAssetImportBrowser(RuntimeImportMode::Auto, "Import Runtime Asset");
        return true;
    }

    void UILayer::appendTerminalLine(const std::string& line) {
        m_terminalLines.push_back(line);
        constexpr std::size_t maxLogLines = 500;
        if (m_terminalLines.size() > maxLogLines) {
            const std::size_t overflow = m_terminalLines.size() - maxLogLines;
            m_terminalLines.erase(m_terminalLines.begin(), m_terminalLines.begin() + static_cast<std::ptrdiff_t>(overflow));
        }

        m_scrollTerminalToBottom = true;
        LOG_INFO("UI: " + line);
    }

    void UILayer::drawEngineSettingsSection() {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Engine Settings");

        if (m_window == nullptr || m_engineConfig == nullptr) {
            ImGui::TextDisabled("Engine settings are not available.");
            return;
        }

        auto [monitorWidth, monitorHeight] = m_window->getPrimaryMonitorSize();
        ImGui::Text("Primary Monitor: %d x %d", monitorWidth, monitorHeight);

        bool changed = false;
        if (ImGui::Checkbox("Auto Detect Monitor Size", &m_editableEngineSettings.autoDetectMonitorSize)) {
            changed = true;
        }

        if (m_editableEngineSettings.autoDetectMonitorSize) {
            ImGui::BeginDisabled();
        }

        if (ImGui::InputInt("Window Width", &m_editableEngineSettings.windowWidth, 64, 256)) {
            changed = true;
        }
        if (ImGui::InputInt("Window Height", &m_editableEngineSettings.windowHeight, 64, 256)) {
            changed = true;
        }

        if (m_editableEngineSettings.autoDetectMonitorSize) {
            ImGui::EndDisabled();
        }

        if (ImGui::Checkbox("Fullscreen", &m_editableEngineSettings.fullscreen)) {
            if (m_editableEngineSettings.fullscreen) {
                m_editableEngineSettings.dockedFullscreenWindowed = false;
            }
            changed = true;
        }

        if (ImGui::Checkbox("Docked Fullscreen (Windowed)", &m_editableEngineSettings.dockedFullscreenWindowed)) {
            if (m_editableEngineSettings.dockedFullscreenWindowed) {
                m_editableEngineSettings.fullscreen = false;
            }
            changed = true;
        }

        if (ImGui::InputText("Window Title", m_windowTitleBuffer.data(), m_windowTitleBuffer.size())) {
            m_editableEngineSettings.windowTitle = std::string(m_windowTitleBuffer.data());
            changed = true;
        }

        if (changed) {
            m_editableEngineSettings.windowWidth = std::max(64, m_editableEngineSettings.windowWidth);
            m_editableEngineSettings.windowHeight = std::max(64, m_editableEngineSettings.windowHeight);
            if (m_editableEngineSettings.windowTitle.empty()) {
                m_editableEngineSettings.windowTitle = "Valkron Engine";
                std::snprintf(m_windowTitleBuffer.data(), m_windowTitleBuffer.size(), "%s", m_editableEngineSettings.windowTitle.c_str());
            }

            m_engineSettingsDirty = true;
        }

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        m_window->getFramebufferSize(framebufferWidth, framebufferHeight);
        ImGui::Text("Current Framebuffer: %d x %d", framebufferWidth, framebufferHeight);

        const bool disableApplyControls = !m_engineSettingsDirty;
        if (disableApplyControls) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Apply and Save")) {
            m_editableEngineSettings.windowTitle = std::string(m_windowTitleBuffer.data());
            if (m_editableEngineSettings.windowTitle.empty()) {
                m_editableEngineSettings.windowTitle = "Valkron Engine";
            }

            m_window->applySettings(m_editableEngineSettings);

            m_engineConfig->setSettings(m_editableEngineSettings);
            if (m_engineConfig->save()) {
                appendTerminalLine("Engine settings applied and saved.");
            } else {
                appendTerminalLine("Failed to save engine settings file.");
            }

            m_engineSettingsDirty = false;

            int resizedFramebufferWidth = 0;
            int resizedFramebufferHeight = 0;
            m_window->getFramebufferSize(resizedFramebufferWidth, resizedFramebufferHeight);
            Renderer::onWindowResize(resizedFramebufferWidth, resizedFramebufferHeight);
        }

        if (disableApplyControls) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Reload from File")) {
            if (m_engineConfig->load()) {
                syncEngineSettingsEditorState();
                m_window->applySettings(m_engineConfig->getSettings());

                int resizedFramebufferWidth = 0;
                int resizedFramebufferHeight = 0;
                m_window->getFramebufferSize(resizedFramebufferWidth, resizedFramebufferHeight);
                Renderer::onWindowResize(resizedFramebufferWidth, resizedFramebufferHeight);

                appendTerminalLine("Engine settings reloaded from config file.");
            } else {
                appendTerminalLine("Failed to reload engine settings from config file.");
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Scene View and Gizmo");

        ImGui::SetNextItemWidth(170.0f);
        ImGui::SliderFloat("Orbit Rotate Speed", &m_sceneCameraRotateSpeed, 0.002f, 0.03f, "%.3f");
        ImGui::SetNextItemWidth(170.0f);
        ImGui::SliderFloat("Pan Speed", &m_sceneCameraPanSpeed, 0.0005f, 0.02f, "%.4f");
        ImGui::SetNextItemWidth(170.0f);
        ImGui::SliderFloat("Zoom Speed", &m_sceneCameraZoomSpeed, 0.02f, 0.40f, "%.2f");
        ImGui::Checkbox("Invert Pan", &m_sceneCameraInvertPan);

        ImGui::SetNextItemWidth(170.0f);
        ImGui::SliderInt("Grid Half Extent", &m_sceneGridHalfExtent, 2, 64);
        ImGui::SetNextItemWidth(170.0f);
        ImGui::SliderFloat("Grid Spacing", &m_sceneGridSpacing, 0.25f, 10.0f, "%.2f");

        ImGui::SetNextItemWidth(170.0f);
        ImGui::SliderFloat("Gizmo Size", &m_gizmoSizeClipSpace, 0.06f, 0.30f, "%.2f");
        ImGui::SetNextItemWidth(170.0f);
        ImGui::SliderFloat("Gizmo Rotation Sensitivity", &m_gizmoRotationSensitivity, 0.25f, 1.5f, "x%.2f");

        if (ImGui::Button("Reset Scene Camera")) {
            m_sceneCameraPivot = glm::vec3(0.0f, 0.0f, 0.0f);
            m_sceneCameraDistance = 2.0f;
            m_sceneCameraYawRadians = 0.0f;
            m_sceneCameraPitchRadians = 0.0f;
            m_sceneCameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
            syncRendererCameraFromController();
        }
    }

    void UILayer::syncEngineSettingsEditorState() {
        if (m_engineConfig == nullptr) {
            return;
        }

        m_editableEngineSettings = m_engineConfig->getSettings();
        std::fill(m_windowTitleBuffer.begin(), m_windowTitleBuffer.end(), '\0');
        std::snprintf(m_windowTitleBuffer.data(), m_windowTitleBuffer.size(), "%s", m_editableEngineSettings.windowTitle.c_str());
        m_engineSettingsDirty = false;
    }

}



