#include "Application/UI/UILayer.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

#include "Core/Log.hpp"
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

    std::string openFileDialogNative(const char* dialogTitle, const char* filterSpec) {
#if defined(_WIN32)
        char selectedPath[MAX_PATH] = {};
        OPENFILENAMEA dialog = {};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = nullptr;
        dialog.lpstrFile = selectedPath;
        dialog.nMaxFile = MAX_PATH;
        dialog.lpstrFilter = filterSpec;
        dialog.nFilterIndex = 1;
        dialog.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        dialog.lpstrTitle = dialogTitle;

        if (GetOpenFileNameA(&dialog) != 0) {
            return std::string(selectedPath);
        }
#endif
        (void)dialogTitle;
        (void)filterSpec;
        return {};
    }

    std::string makeUniqueAssetName(std::string baseName, const std::vector<std::string>& existingNames) {
        if (baseName.empty()) {
            baseName = "Asset";
        }

        std::unordered_set<std::string> existingSet(existingNames.begin(), existingNames.end());
        if (existingSet.find(baseName) == existingSet.end()) {
            return baseName;
        }

        int suffix = 1;
        for (;;) {
            const std::string candidate = baseName + "_" + std::to_string(suffix);
            if (existingSet.find(candidate) == existingSet.end()) {
                return candidate;
            }

            ++suffix;
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

    std::string deriveAssetBaseName(const std::string& absoluteOrRelativePath, const std::string& fallbackName) {
        const std::filesystem::path filePath(absoluteOrRelativePath);
        const std::string stem = filePath.stem().string();
        return stem.empty() ? fallbackName : stem;
    }

    std::vector<std::string> collectAllRuntimeAssetNames() {
        std::vector<std::string> names;

        auto appendNames = [&names](const std::vector<std::string>& sourceNames) {
            names.insert(names.end(), sourceNames.begin(), sourceNames.end());
        };

        appendNames(AssetLoader::getTexture2DNames());
        appendNames(AssetLoader::getTexture3DNames());
        appendNames(AssetLoader::getShaderNames());
        appendNames(AssetLoader::getComputeShaderNames());
        appendNames(AssetLoader::getModelNames());
        return names;
    }

    std::string normalizePathKey(const std::string& pathValue) {
        std::string normalized = std::filesystem::path(pathValue).lexically_normal().generic_string();
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return normalized;
    }

    int registerImportedModelMaterialTextures(Scene& scene, const std::string& modelName) {
        const std::shared_ptr<Model> model = AssetLoader::getModel(modelName);
        if (model == nullptr || !model->isLoaded()) {
            return 0;
        }

        std::vector<std::string> existingNames = collectAllRuntimeAssetNames();
        std::unordered_set<std::string> existingPathKeys;
        for (const SceneAsset& existingAsset : scene.getAssets()) {
            existingPathKeys.insert(normalizePathKey(existingAsset.path));
            existingNames.push_back(existingAsset.name);
        }

        int importedTextureCount = 0;
        for (const std::string& texturePath : model->getReferencedTexturePaths()) {
            const std::string pathKey = normalizePathKey(texturePath);
            if (existingPathKeys.find(pathKey) != existingPathKeys.end()) {
                continue;
            }

            const std::string textureName = makeUniqueAssetName(deriveAssetBaseName(texturePath, modelName + "_Texture"), existingNames);
            if (!AssetLoader::loadTexture2D(textureName, texturePath)) {
                continue;
            }

            scene.addAsset(textureName, texturePath);
            existingNames.push_back(textureName);
            existingPathKeys.insert(pathKey);
            ++importedTextureCount;
        }

        return importedTextureCount;
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
            IM_COL32(23, 28, 41, 135),
            IM_COL32(23, 28, 41, 135),
            IM_COL32(12, 15, 24, 170),
            IM_COL32(12, 15, 24, 170)
        );
        drawList->AddRect(windowPos, windowMax, IM_COL32(54, 63, 84, 170), 0.0f, 0, 1.0f);
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

    bool isTextureImageExtension(const std::string& extension) {
        return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" || extension == ".tga" || extension == ".ppm";
    }

    bool isModelAssetExtension(const std::string& extension) {
        return extension == ".obj" || extension == ".fbx" || extension == ".gltf" || extension == ".glb" || extension == ".dae" || extension == ".3ds";
    }

    bool isComputeShaderExtension(const std::string& extension) {
        return extension == ".comp";
    }

    bool isVertexShaderExtension(const std::string& extension) {
        return extension == ".vert" || extension == ".vs";
    }

    bool isFragmentShaderExtension(const std::string& extension) {
        return extension == ".frag" || extension == ".fs";
    }

    std::optional<std::pair<std::string, std::string>> resolveShaderPair(const std::filesystem::path& selectedPath) {
        const std::filesystem::path parent = selectedPath.parent_path();
        const std::string stem = selectedPath.stem().string();
        const std::string lowerStem = toLowercase(stem);

        std::vector<std::string> candidateStems;
        candidateStems.push_back(stem);

        auto addStemCandidate = [&candidateStems](const std::string& value) {
            if (value.empty()) {
                return;
            }

            if (std::find(candidateStems.begin(), candidateStems.end(), value) == candidateStems.end()) {
                candidateStems.push_back(value);
            }
        };

        if (lowerStem.ends_with("_vert") && stem.size() > 5) {
            addStemCandidate(stem.substr(0, stem.size() - 5));
        }
        if (lowerStem.ends_with("_frag") && stem.size() > 5) {
            addStemCandidate(stem.substr(0, stem.size() - 5));
        }
        if (lowerStem.ends_with("_vs") && stem.size() > 3) {
            addStemCandidate(stem.substr(0, stem.size() - 3));
        }
        if (lowerStem.ends_with("_fs") && stem.size() > 3) {
            addStemCandidate(stem.substr(0, stem.size() - 3));
        }

        for (const std::string& candidateStem : candidateStems) {
            const std::filesystem::path vertexPath = parent / (candidateStem + ".vert");
            const std::filesystem::path fragmentPath = parent / (candidateStem + ".frag");
            if (std::filesystem::exists(vertexPath) && std::filesystem::exists(fragmentPath)) {
                return std::make_pair(vertexPath.string(), fragmentPath.string());
            }

            const std::filesystem::path vertexPathShort = parent / (candidateStem + ".vs");
            const std::filesystem::path fragmentPathShort = parent / (candidateStem + ".fs");
            if (std::filesystem::exists(vertexPathShort) && std::filesystem::exists(fragmentPathShort)) {
                return std::make_pair(vertexPathShort.string(), fragmentPathShort.string());
            }
        }

        const std::string selectedExtension = toLowercase(selectedPath.extension().string());
        if (isVertexShaderExtension(selectedExtension)) {
            for (const std::string fragmentExtension : {".frag", ".fs"}) {
                std::filesystem::path fragmentPath = selectedPath;
                fragmentPath.replace_extension(fragmentExtension);
                if (std::filesystem::exists(fragmentPath)) {
                    return std::make_pair(selectedPath.string(), fragmentPath.string());
                }
            }
        }

        if (isFragmentShaderExtension(selectedExtension)) {
            for (const std::string vertexExtension : {".vert", ".vs"}) {
                std::filesystem::path vertexPath = selectedPath;
                vertexPath.replace_extension(vertexExtension);
                if (std::filesystem::exists(vertexPath)) {
                    return std::make_pair(vertexPath.string(), selectedPath.string());
                }
            }
        }

        return std::nullopt;
    }

    const char kImageFileFilter[] = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.ppm\0All Files\0*.*\0\0";
    const char kAllRuntimeAssetFilter[] = "Supported Assets\0*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.ppm;*.vert;*.vs;*.frag;*.fs;*.comp;*.obj;*.fbx;*.gltf;*.glb;*.dae;*.3ds\0All Files\0*.*\0\0";
    const char kVertexShaderFilter[] = "Vertex Shaders\0*.vert;*.vs;*.glsl\0All Files\0*.*\0\0";
    const char kFragmentShaderFilter[] = "Fragment Shaders\0*.frag;*.fs;*.glsl\0All Files\0*.*\0\0";
    const char kComputeShaderFilter[] = "Compute Shaders\0*.comp;*.glsl\0All Files\0*.*\0\0";
    const char kModelFileFilter[] = "Model Files\0*.obj;*.fbx;*.gltf;*.glb;*.dae;*.3ds\0All Files\0*.*\0\0";
    const char kModelAssetDragPayloadType[] = "AssetBrowser.ModelAssetName";

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

    void UILayer::onAttach() {
        m_activeScene = Scene("Sandbox Scene");

        m_activeScene.addEntity("Camera", SceneEntityType::Camera);
        m_activeScene.addEntity("Directional Light", SceneEntityType::Light);
        m_activeScene.addEntity("Cube", SceneEntityType::Generic);

        if (const std::optional<std::size_t> cameraEntityIndex = m_activeScene.findEntityIndex("Camera"); cameraEntityIndex.has_value()) {
            if (SceneEntity* cameraEntity = m_activeScene.getEntityByIndex(cameraEntityIndex.value()); cameraEntity != nullptr) {
                cameraEntity->transform.position = glm::vec3(0.0f, 1.6f, 4.0f);
                cameraEntity->transform.rotation = glm::vec3(-12.0f, 0.0f, 0.0f);
            }
        }

        if (const std::optional<std::size_t> lightEntityIndex = m_activeScene.findEntityIndex("Directional Light"); lightEntityIndex.has_value()) {
            if (SceneEntity* lightEntity = m_activeScene.getEntityByIndex(lightEntityIndex.value()); lightEntity != nullptr) {
                lightEntity->transform.position = glm::vec3(3.0f, 4.0f, 3.0f);
            }
        }

        if (const std::optional<std::size_t> cubeEntityIndex = m_activeScene.findEntityIndex("Cube"); cubeEntityIndex.has_value()) {
            if (SceneEntity* cubeEntity = m_activeScene.getEntityByIndex(cubeEntityIndex.value()); cubeEntity != nullptr) {
                cubeEntity->transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
                cubeEntity->transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
                cubeEntity->transform.size = glm::vec3(1.0f, 1.0f, 1.0f);
                cubeEntity->modelAssetName = "Test Model";
            }
        }

        m_activeScene.addAsset("checker.ppm", "assets/textures/checker.ppm");
        m_activeScene.addAsset("textured.vert", "assets/shaders/textured.vert");
        m_activeScene.addAsset("textured.frag", "assets/shaders/textured.frag");

        m_activeScene.addScript("SpinController", "scripts/SpinController.lua", true);
        m_activeScene.addScript("CameraOrbit", "scripts/CameraOrbit.lua", false);

        m_activeScene.addCamera("Editor Camera", CameraType::Perspective, true);
        m_activeScene.addCamera("Ortho Debug Camera", CameraType::Orthographic, false);

        m_activeScene.setSetting("Renderer.VSync", "true");
        m_activeScene.setSetting("Renderer.MSAA", "4");
        m_activeScene.setSetting("Physics.Gravity", "0,-9.81,0");

        m_activeScene.setGameStateValue("Mode", "Edit");
        m_activeScene.setGameStateValue("SelectedEntity", "None");

        AssetLoader::initialize();

        auto loadEntityIconTexture = [](const std::string& iconPath) -> std::shared_ptr<Texture> {
            auto texture = std::make_shared<Texture>();
            if (!texture->loadTexture(iconPath, false)) {
                return nullptr;
            }

            return texture;
        };

        m_cameraEntityIconTexture = loadEntityIconTexture("assets/icons/camera.png");
        m_lightEntityIconTexture = loadEntityIconTexture("assets/icons/light.png");

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
        m_showSettingsPanel = false;
        m_showDebugPanel = false;
        m_dockspaceBuilt = false;
        m_gizmoOperationIndex = 0;
        m_gizmoWorldMode = false;

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
        m_inspectorPanelController.reset();
        m_settingsPanelController.reset();
        m_debugPanelController.reset();
        m_assetBrowserPanelController.reset();

        m_cameraEntityIconTexture.reset();
        m_lightEntityIconTexture.reset();
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

        std::optional<glm::mat4> runtimeCameraTransform;
        for (std::size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex) {
            const glm::mat4 worldTransform = composeEntityWorldTransformMatrix(entities, entityIndex);
            const bool cameraEntity = isCameraEntity(entities[entityIndex]);
            const bool lightEntity = isLightEntity(entities[entityIndex]);

            if (lightEntity) {
                lightEntityPositions.push_back(extractWorldPosition(worldTransform));
            }

            if (!runtimeCameraTransform.has_value() && cameraEntity) {
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

        if (m_showBottomPanel) {
            if (m_assetBrowserPanelController != nullptr) {
                m_assetBrowserPanelController->render(deltaTime);
            }
        }

        if (m_showDebugPanel) {
            if (m_debugPanelController != nullptr) {
                m_debugPanelController->render(deltaTime);
            }
        }

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

        if (sceneImageHovered && m_sceneViewOptionsPopupEnabled && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !io.KeyCtrl) {
            ImGui::OpenPopup("SceneView.Options");
        }

        if (ImGui::BeginPopup("SceneView.Options")) {
            ImGui::Text("Scene View Options");
            ImGui::Separator();
            ImGui::TextDisabled("Controls");
            ImGui::BulletText("Middle mouse drag: orbit around pivot");
            ImGui::BulletText("Mouse wheel: zoom in/out");
            ImGui::BulletText("Ctrl + right mouse drag: pan scene");
            ImGui::BulletText("Right click: open this options menu");

            ImGui::Spacing();
            if (ImGui::SliderFloat("Rotate Speed", &m_sceneCameraRotateSpeed, 0.002f, 0.03f, "%.3f")) {
                changedCamera = true;
            }
            if (ImGui::SliderFloat("Pan Speed", &m_sceneCameraPanSpeed, 0.0005f, 0.02f, "%.4f")) {
                changedCamera = true;
            }
            if (ImGui::SliderFloat("Zoom Speed", &m_sceneCameraZoomSpeed, 0.02f, 0.40f, "%.2f")) {
                changedCamera = true;
            }
            ImGui::Checkbox("Invert Pan", &m_sceneCameraInvertPan);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Grid");
            ImGui::Checkbox("Show Grid", &m_showSceneGrid);
            ImGui::SetNextItemWidth(120.0f);
            ImGui::SliderInt("Grid Half Extent", &m_sceneGridHalfExtent, 2, 64);
            ImGui::SetNextItemWidth(120.0f);
            ImGui::SliderFloat("Grid Spacing", &m_sceneGridSpacing, 0.25f, 10.0f, "%.2f");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Gizmo");
            ImGui::BulletText("W: Translate");
            ImGui::BulletText("E: Rotate");
            ImGui::BulletText("R: Scale");

            if (ImGui::Button("Reset Camera")) {
                m_sceneCameraPivot = glm::vec3(0.0f, 0.0f, 0.0f);
                m_sceneCameraDistance = 2.0f;
                m_sceneCameraYawRadians = 0.0f;
                m_sceneCameraPitchRadians = 0.0f;
                m_sceneCameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
                changedCamera = true;
            }

            ImGui::EndPopup();
        }

        if (changedCamera) {
            syncRendererCameraFromController();
        }
    }

    bool UILayer::loadRuntimeTexture2D() {
        const std::string texturePath = openFileDialogNative("Select 2D Texture", kImageFileFilter);
        if (texturePath.empty()) {
            return false;
        }

        const std::vector<std::string> existingNames = collectAllRuntimeAssetNames();
        const std::string textureName = makeUniqueAssetName(deriveAssetBaseName(texturePath, "Texture2D"), existingNames);

        if (!AssetLoader::loadTexture2D(textureName, texturePath)) {
            appendTerminalLine("Failed to import 2D texture: " + texturePath);
            return false;
        }

        m_activeScene.addAsset(textureName, texturePath);
        appendTerminalLine("Imported 2D texture: " + textureName + " from " + texturePath);
        return true;
    }

    bool UILayer::loadRuntimeTexture3D() {
        const std::string texturePath = openFileDialogNative("Select 3D Texture Source", kImageFileFilter);
        if (texturePath.empty()) {
            return false;
        }

        m_runtimeTexture3DDepth = std::max(1, m_runtimeTexture3DDepth);

        const std::vector<std::string> existingNames = collectAllRuntimeAssetNames();
        const std::string textureName = makeUniqueAssetName(deriveAssetBaseName(texturePath, "Texture3D"), existingNames);

        if (!AssetLoader::loadTexture3D(textureName, texturePath, m_runtimeTexture3DDepth)) {
            appendTerminalLine("Failed to import 3D texture: " + texturePath);
            return false;
        }

        m_activeScene.addAsset(textureName, texturePath);
        appendTerminalLine("Imported 3D texture: " + textureName + " (depth " + std::to_string(m_runtimeTexture3DDepth) + ")");
        return true;
    }

    bool UILayer::loadRuntimeShader() {
        const std::string vertexPath = openFileDialogNative("Select Vertex Shader", kVertexShaderFilter);
        if (vertexPath.empty()) {
            return false;
        }

        const std::string fragmentPath = openFileDialogNative("Select Fragment Shader", kFragmentShaderFilter);
        if (fragmentPath.empty()) {
            return false;
        }

        std::string baseName = deriveAssetBaseName(vertexPath, "Shader");
        if (baseName.ends_with("_vert")) {
            baseName = baseName.substr(0, baseName.size() - 5);
        }

        const std::vector<std::string> existingNames = collectAllRuntimeAssetNames();
        const std::string shaderName = makeUniqueAssetName(baseName, existingNames);

        if (!AssetLoader::loadShader(shaderName, vertexPath, fragmentPath)) {
            appendTerminalLine("Failed to import shader pair: " + vertexPath + " + " + fragmentPath);
            return false;
        }

        m_activeScene.addAsset(shaderName + "_vert", vertexPath);
        m_activeScene.addAsset(shaderName + "_frag", fragmentPath);
        appendTerminalLine("Imported shader pair: " + shaderName);
        return true;
    }

    bool UILayer::loadRuntimeComputeShader() {
        const std::string computePath = openFileDialogNative("Select Compute Shader", kComputeShaderFilter);
        if (computePath.empty()) {
            return false;
        }

        const std::vector<std::string> existingNames = collectAllRuntimeAssetNames();
        const std::string shaderName = makeUniqueAssetName(deriveAssetBaseName(computePath, "Compute"), existingNames);

        if (!AssetLoader::loadComputeShader(shaderName, computePath)) {
            appendTerminalLine("Failed to import compute shader: " + computePath);
            return false;
        }

        m_activeScene.addAsset(shaderName, computePath);
        appendTerminalLine("Imported compute shader: " + shaderName);
        return true;
    }

    bool UILayer::loadRuntimeModel() {
        const std::string modelPath = openFileDialogNative("Select Model", kModelFileFilter);
        if (modelPath.empty()) {
            return false;
        }

        const std::vector<std::string> existingNames = collectAllRuntimeAssetNames();
        const std::string modelName = makeUniqueAssetName(deriveAssetBaseName(modelPath, "Model"), existingNames);

        if (!AssetLoader::loadModel(modelName, modelPath)) {
            appendTerminalLine("Failed to import model: " + modelPath);
            return false;
        }

        m_activeScene.addAsset(modelName, modelPath);

        const int importedMaterialTextures = registerImportedModelMaterialTextures(m_activeScene, modelName);
        if (importedMaterialTextures > 0) {
            appendTerminalLine(
                "Imported " + std::to_string(importedMaterialTextures) +
                " material texture(s) for model " + modelName + "."
            );
        }

        appendTerminalLine("Imported model asset: " + modelName + ". Drag it into Scene View to create an entity.");
        return true;
    }

    bool UILayer::loadRuntimeAssetAuto() {
        const std::string assetPath = openFileDialogNative("Import Asset", kAllRuntimeAssetFilter);
        if (assetPath.empty()) {
            return false;
        }

        const std::filesystem::path selectedPath(assetPath);
        const std::string extension = toLowercase(selectedPath.extension().string());
        const std::vector<std::string> existingNames = collectAllRuntimeAssetNames();

        if (isTextureImageExtension(extension)) {
            const std::string lowerPath = toLowercase(assetPath);
            const bool importAsVolume = lowerPath.find("_3d") != std::string::npos || lowerPath.find("volume") != std::string::npos;
            const std::string fallbackName = importAsVolume ? "Texture3D" : "Texture2D";
            const std::string textureName = makeUniqueAssetName(deriveAssetBaseName(assetPath, fallbackName), existingNames);

            const bool loaded = importAsVolume
                ? AssetLoader::loadTexture3D(textureName, assetPath, std::max(1, m_runtimeTexture3DDepth))
                : AssetLoader::loadTexture2D(textureName, assetPath);

            if (!loaded) {
                appendTerminalLine("Failed to import texture asset: " + assetPath);
                return false;
            }

            m_activeScene.addAsset(textureName, assetPath);
            appendTerminalLine("Imported texture: " + textureName + (importAsVolume ? " (3D)" : " (2D)"));
            return true;
        }

        if (isModelAssetExtension(extension)) {
            const std::string modelName = makeUniqueAssetName(deriveAssetBaseName(assetPath, "Model"), existingNames);
            if (!AssetLoader::loadModel(modelName, assetPath)) {
                appendTerminalLine("Failed to import model asset: " + assetPath);
                return false;
            }

            m_activeScene.addAsset(modelName, assetPath);

            const int importedMaterialTextures = registerImportedModelMaterialTextures(m_activeScene, modelName);
            if (importedMaterialTextures > 0) {
                appendTerminalLine(
                    "Imported " + std::to_string(importedMaterialTextures) +
                    " material texture(s) for model " + modelName + "."
                );
            }

            appendTerminalLine("Imported model asset: " + modelName + ". Drag it into Scene View to create an entity.");
            return true;
        }

        const std::string fileNameLower = toLowercase(selectedPath.filename().string());
        if (isComputeShaderExtension(extension) || (extension == ".glsl" && fileNameLower.find("comp") != std::string::npos)) {
            const std::string computeName = makeUniqueAssetName(deriveAssetBaseName(assetPath, "Compute"), existingNames);
            if (!AssetLoader::loadComputeShader(computeName, assetPath)) {
                appendTerminalLine("Failed to import compute shader: " + assetPath);
                return false;
            }

            m_activeScene.addAsset(computeName, assetPath);
            appendTerminalLine("Imported compute shader: " + computeName);
            return true;
        }

        if (isVertexShaderExtension(extension) || isFragmentShaderExtension(extension) || extension == ".glsl") {
            const std::optional<std::pair<std::string, std::string>> shaderPair = resolveShaderPair(selectedPath);
            if (!shaderPair.has_value()) {
                appendTerminalLine("Unable to auto-resolve shader pair. Select a .vert/.frag (or .vs/.fs) file with matching sibling file.");
                return false;
            }

            std::string baseName = deriveAssetBaseName(shaderPair->first, "Shader");
            if (baseName.ends_with("_vert")) {
                baseName = baseName.substr(0, baseName.size() - 5);
            }

            const std::string shaderName = makeUniqueAssetName(baseName, existingNames);
            if (!AssetLoader::loadShader(shaderName, shaderPair->first, shaderPair->second)) {
                appendTerminalLine("Failed to import shader pair: " + shaderPair->first + " + " + shaderPair->second);
                return false;
            }

            m_activeScene.addAsset(shaderName + "_vert", shaderPair->first);
            m_activeScene.addAsset(shaderName + "_frag", shaderPair->second);
            appendTerminalLine("Imported shader pair: " + shaderName);
            return true;
        }

        appendTerminalLine("Unsupported asset type for import: " + assetPath);
        return false;
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



