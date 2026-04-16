#pragma once

#include "Application/UI/UILayer.hpp"

#include "Engine/AssetLoader.hpp"
#include "Renderer/Model.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/Texture.hpp"

#include "imgui.h"
#include "imgui_internal.h"
#include "ImGuizmo.h"

#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace Valkron {

    const char* sceneStateToString(SceneState state);
    std::string toLowercase(std::string value);
    std::string deriveAssetBaseName(const std::string& absoluteOrRelativePath, const std::string& fallbackName);
    std::string normalizeFolderPath(const std::string& pathValue);
    std::string getAssetFolderPath(const SceneAsset& asset);
    std::string getFolderLeafName(const std::string& folderPath);
    std::string getParentFolderPath(const std::string& folderPath);
    std::optional<std::string> getDirectChildFolder(const std::string& currentFolder, const std::string& candidateFolder);
    bool isModelSceneAsset(const SceneAsset& asset);
    const char* getAssetIconToken(const SceneAsset& asset);
    ImVec4 getAssetIconColor(const SceneAsset& asset);
    bool isCameraEntity(const SceneEntity& entity);
    bool isLightEntity(const SceneEntity& entity);
    const char* getSceneEntityTypeDisplayName(SceneEntityType type);
    const char* getEntityCategoryToken(const SceneEntity& entity);
    const char* getEntityIconToken(const SceneEntity& entity);
    ImVec4 getEntityBadgeColor(const SceneEntity& entity);
    ImVec4 brightenColor(const ImVec4& color, float amount);
    void ensureEntityUsesPbrComponent(SceneEntity& entity);
    glm::mat4 composeEntityWorldTransformMatrix(const std::vector<SceneEntity>& entities, std::size_t entityIndex);
    glm::vec3 extractWorldPosition(const glm::mat4& worldMatrix);
    std::optional<ImVec2> projectWorldPositionToImage(
        const glm::vec3& worldPosition,
        const glm::mat4& viewMatrix,
        const glm::mat4& projectionMatrix,
        const ImVec2& imageRectMin,
        const ImVec2& imageRectMax
    );
    std::optional<std::pair<ImVec2, ImVec2>> projectWorldLineToImageClipped(
        const glm::vec3& startWorld,
        const glm::vec3& endWorld,
        const glm::mat4& viewMatrix,
        const glm::mat4& projectionMatrix,
        const ImVec2& imageRectMin,
        const ImVec2& imageRectMax,
        float nearClipDistance
    );
    ImGuizmo::OPERATION getGizmoOperationFromIndex(int operationIndex);
    ImGuizmo::MODE getGizmoMode(bool worldMode);
    void applyMatrixToSceneTransform(const glm::mat4& matrix, SceneTransform& transform, ImGuizmo::OPERATION operation, float rotationSensitivity);
    void drawWindowPanelGradient();
    void drawHorizontalSceneGrid(
        ImDrawList* drawList,
        const glm::mat4& viewMatrix,
        const glm::mat4& projectionMatrix,
        const ImVec2& imageRectMin,
        const ImVec2& imageRectMax,
        int halfExtent,
        float spacing
    );
    bool drawColoredVec3Control(const char* label, glm::vec3& value, float speed, bool hasLimits, float minValue, float maxValue);
    std::optional<float> rayAabbIntersectionDistance(
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection,
        const glm::vec3& boundsMin,
        const glm::vec3& boundsMax
    );
    std::optional<float> raySphereIntersectionDistance(
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection,
        const glm::vec3& sphereCenter,
        float sphereRadius
    );

    constexpr char kModelAssetDragPayloadType[] = "AssetBrowser.ModelAssetName";

}
