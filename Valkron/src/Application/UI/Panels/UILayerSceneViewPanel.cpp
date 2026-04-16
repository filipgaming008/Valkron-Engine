#include "Application/UI/Panels/UILayerPanelsInternal.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <cctype>
#include <filesystem>

#define IMVIEWGUIZMO_IMPLEMENTATION
#define ImLengthSqr ImViewGuizmo_ImLengthSqr
#include "ImViewGuizmo.h"
#undef ImLengthSqr

#include "glm/gtx/quaternion.hpp"

namespace Valkron {

    namespace {

        struct ImportedModelHierarchyNode {
            std::string nodeName;
            std::optional<std::size_t> parentNodeIndex;
            SceneTransform localTransform{};
            std::vector<int> meshIndices;
        };

        std::string sanitizeHierarchyEntityName(std::string value) {
            if (value.empty()) {
                return value;
            }

            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                if (ch == '\\' || ch == '/' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|') {
                    return static_cast<char>('_');
                }

                if (std::iscntrl(ch)) {
                    return static_cast<char>('_');
                }

                return static_cast<char>(ch);
            });

            return value;
        }

        SceneTransform convertAssimpTransformToSceneTransform(const aiMatrix4x4& assimpTransform) {
            aiVector3D scaling;
            aiQuaternion rotation;
            aiVector3D position;
            assimpTransform.Decompose(scaling, rotation, position);

            SceneTransform transform{};
            transform.position = glm::vec3(position.x, position.y, position.z);

            glm::quat orientation(rotation.w, rotation.x, rotation.y, rotation.z);
            if (glm::length(orientation) > 0.0001f) {
                orientation = glm::normalize(orientation);
            } else {
                orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            }

            const glm::mat4 rotationMatrix = glm::mat4_cast(orientation);
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

            transform.rotation = glm::degrees(glm::vec3(xRadians, yRadians, zRadians));
            transform.scale = glm::vec3(scaling.x, scaling.y, scaling.z);
            return transform;
        }

        void collectImportedModelHierarchyNodes(
            const aiNode* node,
            std::optional<std::size_t> parentNodeIndex,
            const std::string& fallbackModelName,
            std::vector<ImportedModelHierarchyNode>& outNodes
        ) {
            if (node == nullptr) {
                return;
            }

            const std::size_t nodeIndex = outNodes.size();

            ImportedModelHierarchyNode hierarchyNode{};
            hierarchyNode.parentNodeIndex = parentNodeIndex;
            hierarchyNode.localTransform = convertAssimpTransformToSceneTransform(node->mTransformation);
            hierarchyNode.meshIndices.reserve(node->mNumMeshes);
            for (unsigned int meshPosition = 0; meshPosition < node->mNumMeshes; ++meshPosition) {
                hierarchyNode.meshIndices.push_back(static_cast<int>(node->mMeshes[meshPosition]));
            }

            if (node->mName.length > 0 && node->mName.C_Str() != nullptr) {
                hierarchyNode.nodeName = sanitizeHierarchyEntityName(node->mName.C_Str());
            }

            if (hierarchyNode.nodeName.empty()) {
                hierarchyNode.nodeName = sanitizeHierarchyEntityName(
                    fallbackModelName + "_Node_" + std::to_string(nodeIndex)
                );
            }

            outNodes.push_back(std::move(hierarchyNode));

            for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
                collectImportedModelHierarchyNodes(node->mChildren[childIndex], nodeIndex, fallbackModelName, outNodes);
            }
        }

        bool buildImportedModelHierarchyNodes(
            const std::string& modelPath,
            const std::string& modelName,
            std::vector<ImportedModelHierarchyNode>& outNodes,
            std::string& outError
        ) {
            outNodes.clear();
            outError.clear();

            Assimp::Importer importer;
            const aiScene* scene = importer.ReadFile(
                modelPath,
                aiProcess_Triangulate |
                aiProcess_GenSmoothNormals |
                aiProcess_FlipUVs |
                aiProcess_JoinIdenticalVertices |
                aiProcess_CalcTangentSpace
            );

            if (scene == nullptr || scene->mRootNode == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0) {
                outError = importer.GetErrorString();
                return false;
            }

            collectImportedModelHierarchyNodes(scene->mRootNode, std::nullopt, deriveAssetBaseName(modelName, "Model"), outNodes);
            return !outNodes.empty();
        }

    }  // namespace

    void UILayer::drawSceneViewPanel(float deltaTime) {
        const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (!ImGui::Begin("Scene View", &m_showSceneViewPanel, windowFlags)) {
            ImGui::End();
            return;
        }

        drawWindowPanelGradient();

        const auto& entities = m_activeScene.getEntityData();
        const bool hasSelectedEntity = m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(entities.size());
        std::optional<SceneEntityType> pendingCreateEntityFromSceneContext;

        ImGui::TextDisabled("Scene: %s", m_activeScene.getName().c_str());

        m_gizmoTranslateSnap = std::max(0.01f, m_gizmoTranslateSnap);
        m_gizmoRotateSnapDegrees = std::max(0.25f, m_gizmoRotateSnapDegrees);
        m_gizmoScaleSnap = std::max(0.01f, m_gizmoScaleSnap);

        ImGui::Spacing();
        const ImVec2 availableRegion = ImGui::GetContentRegionAvail();

        if (availableRegion.x > 2.0f && availableRegion.y > 2.0f) {
            const int desiredViewportWidth = std::max(1, static_cast<int>(availableRegion.x));
            const int desiredViewportHeight = std::max(1, static_cast<int>(availableRegion.y));

            const bool viewportSizeChanged = desiredViewportWidth != m_pendingViewportWidth || desiredViewportHeight != m_pendingViewportHeight;
            if (viewportSizeChanged) {
                m_pendingViewportWidth = desiredViewportWidth;
                m_pendingViewportHeight = desiredViewportHeight;
                m_viewportResizeDebounceTimer = 0.0f;
            } else {
                m_viewportResizeDebounceTimer += std::max(0.0f, deltaTime);
            }

            const int currentViewportWidth = Renderer::getViewportWidth();
            const int currentViewportHeight = Renderer::getViewportHeight();
            const bool needsViewportResize = currentViewportWidth != m_pendingViewportWidth || currentViewportHeight != m_pendingViewportHeight;

            if (needsViewportResize && (
                m_viewportResizeDebounceTimer >= m_viewportResizeDebounceDelaySeconds ||
                currentViewportWidth <= 0 ||
                currentViewportHeight <= 0
            )) {
                Renderer::setViewportSize(m_pendingViewportWidth, m_pendingViewportHeight);
                m_viewportResizeDebounceTimer = 0.0f;
            }
        }

        const unsigned int frameTextureID = Renderer::getSceneFrameTextureID();
        const int renderWidth = Renderer::getViewportWidth();
        const int renderHeight = Renderer::getViewportHeight();

        if (frameTextureID == 0 || renderWidth <= 0 || renderHeight <= 0 || availableRegion.x <= 2.0f || availableRegion.y <= 2.0f) {
            m_sceneViewImageHovered = false;
            ImGui::TextDisabled("Renderer output is not ready yet.");
            ImGui::TextWrapped("Camera controls: MMB orbit, wheel zoom, Ctrl+RMB pan, RMB options.");
            ImGui::End();
            return;
        }

        const float renderAspect = static_cast<float>(renderWidth) / static_cast<float>(renderHeight);
        float imageWidth = availableRegion.x;
        float imageHeight = imageWidth / renderAspect;

        if (imageHeight > availableRegion.y) {
            imageHeight = availableRegion.y;
            imageWidth = imageHeight * renderAspect;
        }

        const float xOffset = (availableRegion.x - imageWidth) * 0.5f;
        const float yOffset = (availableRegion.y - imageHeight) * 0.5f;
        if (xOffset > 0.0f) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + xOffset);
        }
        if (yOffset > 0.0f) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOffset);
        }

        const ImTextureID textureHandle = (ImTextureID)(uintptr_t)frameTextureID;
        ImGui::Image(textureHandle, ImVec2(imageWidth, imageHeight), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

        const ImVec2 imageRectMin = ImGui::GetItemRectMin();
        const ImVec2 imageRectMax = ImGui::GetItemRectMax();
        bool imageClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        m_sceneViewImageHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);

        const float gizmoInset = 94.0f;
        const ImVec2 gizmoPos(imageRectMax.x - gizmoInset, imageRectMin.y + 41.0f);

        {
            ImViewGuizmo::BeginFrame();
            ImViewGuizmo::Style& viewGizmoStyle = ImViewGuizmo::GetStyle();
            viewGizmoStyle.scale = 0.50f;
            viewGizmoStyle.bigCircleRadius = 62.0f;
            viewGizmoStyle.lineWidth = 3.0f;
            viewGizmoStyle.circleRadius = 12.0f;
            viewGizmoStyle.toolButtonRadius = 18.0f;
            viewGizmoStyle.labelSize = 0.90f;

            const glm::vec3 cameraPos = Renderer::getCameraPosition();
            const glm::vec3 pivot = Renderer::getCameraTarget();
            const glm::vec3 toCamera = cameraPos - pivot;

            if (glm::length(toCamera) > 0.0001f) {
                glm::vec3 viewDir = glm::normalize(toCamera);
                glm::quat viewRot = glm::quatLookAt(viewDir, m_sceneCameraUp);
                glm::vec3 mutablePos = cameraPos;

                bool cameraChanged = false;
                cameraChanged |= ImViewGuizmo::Rotate(mutablePos, viewRot, pivot, gizmoPos, m_sceneCameraRotateSpeed);
                cameraChanged |= ImViewGuizmo::Dolly(mutablePos, viewRot, ImVec2(gizmoPos.x - 20.0f, gizmoPos.y + 118.0f), m_sceneCameraZoomSpeed);
                cameraChanged |= ImViewGuizmo::Pan(mutablePos, viewRot, ImVec2(gizmoPos.x + 20.0f, gizmoPos.y + 118.0f), m_sceneCameraPanSpeed);

                if (cameraChanged) {
                    const glm::vec3 newOffset = mutablePos - pivot;
                    const float newDistance = glm::length(newOffset);
                    if (newDistance > 0.0001f) {
                        m_sceneCameraPivot = pivot;
                        m_sceneCameraDistance = std::max(0.15f, newDistance);
                        m_sceneCameraPitchRadians = std::clamp(std::asin(newOffset.y / m_sceneCameraDistance), -1.52f, 1.52f);
                        m_sceneCameraYawRadians = std::atan2(newOffset.x, newOffset.z);
                        syncRendererCameraFromController();
                    }
                }
            }
        }

        if (m_sceneViewImageHovered) {
            if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
                m_gizmoOperationIndex = 0;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
                m_gizmoOperationIndex = 1;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
                m_gizmoOperationIndex = 2;
            }
        }

        bool gizmoToolsHovered = false;
        {
            const float toolsRightMargin = 8.0f;
            const float desiredToolsY = gizmoPos.y + 154.0f;
            const float minToolsY = imageRectMin.y + 62.0f;
            const float maxToolsY = std::max(minToolsY, imageRectMax.y - 220.0f);
            const float toolsWindowY = std::clamp(desiredToolsY, minToolsY, maxToolsY);
            const ImVec2 toolsWindowPos(imageRectMax.x - toolsRightMargin, toolsWindowY);
            ImGui::SetNextWindowPos(toolsWindowPos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.92f);

            const ImGuiWindowFlags toolsWindowFlags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoDocking |
                ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_AlwaysAutoResize;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(22, 25, 30, 235));
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(118, 122, 132, 205));

            if (ImGui::Begin("##SceneViewTransformTools", nullptr, toolsWindowFlags)) {
                ImGui::TextUnformatted("Transform");
                ImGui::Separator();

                if (ImGui::RadioButton("Translate (W)", m_gizmoOperationIndex == 0)) {
                    m_gizmoOperationIndex = 0;
                }
                if (ImGui::RadioButton("Rotate (E)", m_gizmoOperationIndex == 1)) {
                    m_gizmoOperationIndex = 1;
                }
                if (ImGui::RadioButton("Scale (R)", m_gizmoOperationIndex == 2)) {
                    m_gizmoOperationIndex = 2;
                }

                ImGui::Separator();
                ImGui::Checkbox("Grid", &m_showSceneGrid);
                ImGui::Checkbox("World Space", &m_gizmoWorldMode);
                ImGui::Checkbox("Snap", &m_gizmoSnapEnabled);

                if (m_gizmoSnapEnabled) {
                    if (m_gizmoOperationIndex == 1) {
                        ImGui::SetNextItemWidth(120.0f);
                        ImGui::DragFloat("Degrees", &m_gizmoRotateSnapDegrees, 0.25f, 0.25f, 90.0f, "%.1f");
                    } else if (m_gizmoOperationIndex == 2) {
                        ImGui::SetNextItemWidth(120.0f);
                        ImGui::DragFloat("Scale Step", &m_gizmoScaleSnap, 0.01f, 0.01f, 10.0f, "%.2f");
                    } else {
                        ImGui::SetNextItemWidth(120.0f);
                        ImGui::DragFloat("Move Step", &m_gizmoTranslateSnap, 0.01f, 0.01f, 10.0f, "%.2f");
                    }
                }

                if (hasSelectedEntity) {
                    ImGui::Separator();
                    ImGui::TextDisabled("Selected: %s", entities[static_cast<std::size_t>(m_selectedEntityIndex)].name.c_str());
                }

                gizmoToolsHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);
            }
            ImGui::End();

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
        }

        if (gizmoToolsHovered) {
            m_sceneViewImageHovered = false;
            imageClicked = false;
        }

        if (m_sceneViewImageHovered && m_sceneViewOptionsPopupEnabled && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !ImGui::GetIO().KeyCtrl) {
            ImGui::OpenPopup("SceneView.ContextMenu");
        }

        if (ImGui::BeginPopup("SceneView.ContextMenu")) {
            ImGui::TextDisabled("Add Entity");
            if (ImGui::MenuItem("Empty Entity")) {
                pendingCreateEntityFromSceneContext = SceneEntityType::Generic;
            }
            if (ImGui::MenuItem("Camera Entity")) {
                pendingCreateEntityFromSceneContext = SceneEntityType::Camera;
            }
            if (ImGui::MenuItem("Light Entity")) {
                pendingCreateEntityFromSceneContext = SceneEntityType::Light;
            }
            ImGui::Separator();
            ImGui::TextDisabled("Camera: MMB orbit, wheel zoom, Ctrl+RMB pan");
            ImGui::EndPopup();
        }

        if (pendingCreateEntityFromSceneContext.has_value()) {
            const SceneEntityType entityType = pendingCreateEntityFromSceneContext.value();
            std::string baseEntityName = "Entity";
            if (entityType == SceneEntityType::Camera) {
                baseEntityName = "Camera";
            } else if (entityType == SceneEntityType::Light) {
                baseEntityName = "Light";
            }

            const std::string entityName = m_activeScene.makeUniqueEntityName(baseEntityName);
            m_activeScene.addEntity(entityName, entityType);

            if (const std::optional<std::size_t> entityIndex = m_activeScene.findEntityIndex(entityName); entityIndex.has_value()) {
                if (SceneEntity* entity = m_activeScene.getEntityByIndex(entityIndex.value()); entity != nullptr) {
                    entity->transform.position = m_runtimeEntityCameraActive ? Renderer::getCameraTarget() : m_sceneCameraPivot;
                }

                if (entityType == SceneEntityType::Camera && m_activeScene.getGameStateValue("PrimaryCameraEntity").value_or("").empty()) {
                    m_activeScene.setGameStateValue("PrimaryCameraEntity", entityName);
                }

                setSelectedEntity(static_cast<int>(entityIndex.value()));
            }

            appendTerminalLine("Created " + entityName + " from Scene View context menu.");
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kModelAssetDragPayloadType)) {
                if (payload->Data != nullptr && payload->DataSize > 0) {
                    const std::string modelName(static_cast<const char*>(payload->Data));
                    if (!modelName.empty()) {
                        std::string modelAssetPath;
                        {
                            const auto& assets = m_activeScene.getAssets();
                            const auto assetIt = std::find_if(assets.begin(), assets.end(), [&modelName](const SceneAsset& asset) {
                                return asset.name == modelName && isModelSceneAsset(asset);
                            });

                            if (assetIt != assets.end()) {
                                modelAssetPath = assetIt->path;
                            }
                        }

                        std::shared_ptr<Model> model = AssetLoader::getModel(modelName);
                        if (model == nullptr || !model->isLoaded()) {
                            if (!modelAssetPath.empty()) {
                                AssetLoader::loadModel(modelName, modelAssetPath);
                                model = AssetLoader::getModel(modelName);
                            }
                        }

                        if (model != nullptr && model->isLoaded()) {
                            const glm::vec3 placementPosition = m_runtimeEntityCameraActive ? Renderer::getCameraTarget() : m_sceneCameraPivot;
                            bool placedAsHierarchy = false;

                            const std::string modelExtension = toLowercase(std::filesystem::path(modelAssetPath).extension().string());
                            if (!modelAssetPath.empty() && modelExtension == ".fbx") {
                                std::vector<ImportedModelHierarchyNode> importedHierarchyNodes;
                                std::string hierarchyImportError;
                                if (buildImportedModelHierarchyNodes(modelAssetPath, modelName, importedHierarchyNodes, hierarchyImportError)) {
                                    if (importedHierarchyNodes.size() > 1) {
                                        std::vector<std::size_t> sceneEntityIndices(importedHierarchyNodes.size(), std::numeric_limits<std::size_t>::max());
                                        for (std::size_t nodeIndex = 0; nodeIndex < importedHierarchyNodes.size(); ++nodeIndex) {
                                            const ImportedModelHierarchyNode& importedNode = importedHierarchyNodes[nodeIndex];
                                            const std::string baseEntityName = importedNode.nodeName.empty()
                                                ? deriveAssetBaseName(modelName, "ModelNode")
                                                : importedNode.nodeName;

                                            const std::string entityName = m_activeScene.makeUniqueEntityName(baseEntityName);
                                            m_activeScene.addEntity(entityName, SceneEntityType::Generic);

                                            const std::optional<std::size_t> entityIndex = m_activeScene.findEntityIndex(entityName);
                                            if (!entityIndex.has_value()) {
                                                continue;
                                            }

                                            sceneEntityIndices[nodeIndex] = entityIndex.value();
                                            if (SceneEntity* entity = m_activeScene.getEntityByIndex(entityIndex.value()); entity != nullptr) {
                                                entity->transform = importedNode.localTransform;

                                                if (!importedNode.meshIndices.empty()) {
                                                    entity->modelAssetName = modelName;
                                                    entity->modelMeshIndices = importedNode.meshIndices;
                                                    entity->applyModelNodeTransforms = false;
                                                    ensureEntityUsesPbrComponent(*entity);
                                                } else {
                                                    entity->modelAssetName.clear();
                                                    entity->modelMeshIndices.clear();
                                                    entity->applyModelNodeTransforms = true;
                                                }
                                            }
                                        }

                                        for (std::size_t nodeIndex = 0; nodeIndex < importedHierarchyNodes.size(); ++nodeIndex) {
                                            const ImportedModelHierarchyNode& importedNode = importedHierarchyNodes[nodeIndex];
                                            if (!importedNode.parentNodeIndex.has_value()) {
                                                continue;
                                            }

                                            const std::size_t childEntityIndex = sceneEntityIndices[nodeIndex];
                                            const std::size_t parentEntityIndex = sceneEntityIndices[importedNode.parentNodeIndex.value()];
                                            if (childEntityIndex == std::numeric_limits<std::size_t>::max() || parentEntityIndex == std::numeric_limits<std::size_t>::max()) {
                                                continue;
                                            }

                                            (void)m_activeScene.setEntityParent(childEntityIndex, parentEntityIndex);
                                        }

                                        const std::size_t rootEntityIndex = sceneEntityIndices.front();
                                        if (rootEntityIndex != std::numeric_limits<std::size_t>::max()) {
                                            if (SceneEntity* rootEntity = m_activeScene.getEntityByIndex(rootEntityIndex); rootEntity != nullptr) {
                                                rootEntity->transform.position += placementPosition;
                                            }

                                            setSelectedEntity(static_cast<int>(rootEntityIndex));
                                        }

                                        appendTerminalLine(
                                            "Placed hierarchical FBX entity tree for " + modelName +
                                            " (" + std::to_string(importedHierarchyNodes.size()) + " nodes)."
                                        );
                                        placedAsHierarchy = true;
                                    }
                                } else if (!hierarchyImportError.empty()) {
                                    appendTerminalLine(
                                        "FBX hierarchy import fallback for " + modelName + ": " + hierarchyImportError
                                    );
                                }
                            }

                            if (!placedAsHierarchy) {
                                const std::string entityName = m_activeScene.makeUniqueEntityName(deriveAssetBaseName(modelName, "Model"));
                                m_activeScene.addEntity(entityName, SceneEntityType::Generic);

                                const std::optional<std::size_t> entityIndex = m_activeScene.findEntityIndex(entityName);
                                if (entityIndex.has_value()) {
                                    if (SceneEntity* entity = m_activeScene.getEntityByIndex(entityIndex.value()); entity != nullptr) {
                                        entity->modelAssetName = modelName;
                                        entity->modelMeshIndices.clear();
                                        entity->applyModelNodeTransforms = true;
                                        entity->transform.position = placementPosition;
                                        entity->transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
                                        entity->transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
                                        entity->transform.size = glm::vec3(1.0f, 1.0f, 1.0f);
                                        ensureEntityUsesPbrComponent(*entity);
                                    }

                                    setSelectedEntity(static_cast<int>(entityIndex.value()));
                                }

                                appendTerminalLine("Placed model entity " + entityName + " from asset " + modelName + ".");
                            }
                        } else {
                            appendTerminalLine("Unable to place model asset: " + modelName + ".");
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        struct OverlayIconHitZone {
            int entityIndex = -1;
            ImVec2 min;
            ImVec2 max;
        };

        std::vector<OverlayIconHitZone> overlayIconHitZones;

        {
            const auto& currentEntities = m_activeScene.getEntityData();
            const glm::mat4 viewMatrix = Renderer::getCameraViewMatrix();
            const glm::mat4 projectionMatrix = Renderer::getCameraProjectionMatrix();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            overlayIconHitZones.reserve(currentEntities.size());

            drawList->PushClipRect(imageRectMin, imageRectMax, true);

            if (m_showSceneGrid) {
                drawHorizontalSceneGrid(
                    drawList,
                    viewMatrix,
                    projectionMatrix,
                    imageRectMin,
                    imageRectMax,
                    m_sceneGridHalfExtent,
                    m_sceneGridSpacing
                );
            }

            for (std::size_t entityIndex = 0; entityIndex < currentEntities.size(); ++entityIndex) {
                const SceneEntity& entity = currentEntities[entityIndex];
                const bool cameraEntity = isCameraEntity(entity);
                const bool lightEntity = isLightEntity(entity);
                if (!cameraEntity && !lightEntity) {
                    continue;
                }

                const glm::mat4 worldTransform = composeEntityWorldTransformMatrix(currentEntities, entityIndex);
                const glm::vec3 worldPosition = extractWorldPosition(worldTransform);
                const std::optional<ImVec2> projectedPosition = projectWorldPositionToImage(
                    worldPosition,
                    viewMatrix,
                    projectionMatrix,
                    imageRectMin,
                    imageRectMax
                );

                if (!projectedPosition.has_value()) {
                    continue;
                }

                ImVec4 iconColor = getEntityBadgeColor(entity);
                const bool selected = static_cast<int>(entityIndex) == m_selectedEntityIndex;
                if (selected) {
                    iconColor = brightenColor(iconColor, 0.12f);
                }

                const std::shared_ptr<Texture>& iconTexture = cameraEntity ? m_cameraEntityIconTexture : m_lightEntityIconTexture;
                const unsigned int iconTextureID = (iconTexture != nullptr) ? iconTexture->getID() : 0u;

                if (iconTextureID != 0) {
                    const float iconSize = selected ? (m_sceneEntityIconPixels + 4.0f) : m_sceneEntityIconPixels;
                    const ImVec2 iconMin(
                        projectedPosition->x - iconSize * 0.5f,
                        projectedPosition->y - iconSize * 0.5f
                    );
                    const ImVec2 iconMax(
                        projectedPosition->x + iconSize * 0.5f,
                        projectedPosition->y + iconSize * 0.5f
                    );

                    drawList->AddRectFilled(
                        ImVec2(iconMin.x - 2.0f, iconMin.y - 2.0f),
                        ImVec2(iconMax.x + 2.0f, iconMax.y + 2.0f),
                        IM_COL32(12, 12, 16, 170),
                        4.0f
                    );
                    drawList->AddImage(
                        (ImTextureID)(uintptr_t)iconTextureID,
                        iconMin,
                        iconMax,
                        ImVec2(0.0f, 0.0f),
                        ImVec2(1.0f, 1.0f),
                        IM_COL32(255, 255, 255, 255)
                    );
                    drawList->AddRect(
                        iconMin,
                        iconMax,
                        ImGui::GetColorU32(selected ? ImVec4(0.98f, 0.78f, 0.24f, 1.0f) : brightenColor(iconColor, 0.10f)),
                        4.0f,
                        0,
                        selected ? 2.0f : 1.0f
                    );

                    overlayIconHitZones.push_back(OverlayIconHitZone{
                        static_cast<int>(entityIndex),
                        ImVec2(iconMin.x - 5.0f, iconMin.y - 5.0f),
                        ImVec2(iconMax.x + 5.0f, iconMax.y + 5.0f)
                    });
                } else {
                    const char* iconToken = cameraEntity ? "[@]" : "[*]";
                    const ImVec2 textSize = ImGui::CalcTextSize(iconToken);
                    const float padX = 6.0f;
                    const float padY = 3.0f;

                    const ImVec2 badgeMin(
                        projectedPosition->x - (textSize.x * 0.5f) - padX,
                        projectedPosition->y - (textSize.y * 0.5f) - padY
                    );
                    const ImVec2 badgeMax(
                        projectedPosition->x + (textSize.x * 0.5f) + padX,
                        projectedPosition->y + (textSize.y * 0.5f) + padY
                    );

                    drawList->AddRectFilled(badgeMin, badgeMax, ImGui::GetColorU32(iconColor), 3.0f);
                    drawList->AddRect(badgeMin, badgeMax, ImGui::GetColorU32(selected ? ImVec4(0.98f, 0.78f, 0.24f, 1.0f) : ImVec4(0.10f, 0.10f, 0.10f, 0.95f)), 3.0f, 0, selected ? 2.0f : 1.0f);

                    const ImVec2 textPosition(
                        projectedPosition->x - textSize.x * 0.5f,
                        projectedPosition->y - textSize.y * 0.5f
                    );
                    drawList->AddText(textPosition, IM_COL32(245, 245, 248, 255), iconToken);

                    overlayIconHitZones.push_back(OverlayIconHitZone{
                        static_cast<int>(entityIndex),
                        ImVec2(badgeMin.x - 4.0f, badgeMin.y - 4.0f),
                        ImVec2(badgeMax.x + 4.0f, badgeMax.y + 4.0f)
                    });
                }

                const bool directionalLightEntity = lightEntity && entity.lightComponent.type == SceneLightType::Directional;
                const bool drawForwardArrow = selected && (cameraEntity || directionalLightEntity);
                if (drawForwardArrow) {
                    glm::vec3 forwardDirection = glm::mat3(worldTransform) * glm::vec3(0.0f, 0.0f, -1.0f);
                    if (glm::length(forwardDirection) <= 0.0001f) {
                        forwardDirection = glm::vec3(0.0f, 0.0f, -1.0f);
                    } else {
                        forwardDirection = glm::normalize(forwardDirection);
                    }

                    if (directionalLightEntity) {
                        const bool hasExplicitRotation = glm::length(entity.transform.rotation) > 0.001f;
                        if (!hasExplicitRotation && glm::length(worldPosition) > 0.001f) {
                            forwardDirection = glm::normalize(-worldPosition);
                        }
                    }

                    const glm::vec3 cameraPosition = Renderer::getCameraPosition();
                    const float distanceToCamera = glm::length(cameraPosition - worldPosition);
                    const float arrowLengthWorld = std::clamp(distanceToCamera * 0.10f, 0.35f, 3.5f);
                    const glm::vec3 arrowTipWorld = worldPosition + forwardDirection * arrowLengthWorld;

                    const std::optional<std::pair<ImVec2, ImVec2>> projectedArrow = projectWorldLineToImageClipped(
                        worldPosition,
                        arrowTipWorld,
                        viewMatrix,
                        projectionMatrix,
                        imageRectMin,
                        imageRectMax,
                        0.02f
                    );

                    if (projectedArrow.has_value()) {
                        const ImVec2 arrowStart = projectedArrow->first;
                        const ImVec2 arrowEnd = projectedArrow->second;

                        const float dx = arrowEnd.x - arrowStart.x;
                        const float dy = arrowEnd.y - arrowStart.y;
                        const float lineLength = std::sqrt(dx * dx + dy * dy);
                        if (lineLength > 2.0f) {
                            const ImVec2 direction(dx / lineLength, dy / lineLength);
                            const ImVec2 perpendicular(-direction.y, direction.x);
                            const float arrowHeadLength = std::clamp(lineLength * 0.30f, 8.0f, 16.0f);
                            const float arrowHeadHalfWidth = arrowHeadLength * 0.45f;

                            const ImVec2 arrowHeadLeft(
                                arrowEnd.x - direction.x * arrowHeadLength + perpendicular.x * arrowHeadHalfWidth,
                                arrowEnd.y - direction.y * arrowHeadLength + perpendicular.y * arrowHeadHalfWidth
                            );
                            const ImVec2 arrowHeadRight(
                                arrowEnd.x - direction.x * arrowHeadLength - perpendicular.x * arrowHeadHalfWidth,
                                arrowEnd.y - direction.y * arrowHeadLength - perpendicular.y * arrowHeadHalfWidth
                            );

                            const ImU32 arrowColor = cameraEntity
                                ? IM_COL32(106, 184, 255, 245)
                                : IM_COL32(255, 224, 118, 245);
                            const ImU32 arrowOutlineColor = IM_COL32(18, 20, 22, 210);

                            drawList->AddLine(arrowStart, arrowEnd, arrowOutlineColor, 4.4f);
                            drawList->AddLine(arrowStart, arrowEnd, arrowColor, 2.6f);
                            drawList->AddTriangleFilled(arrowEnd, arrowHeadLeft, arrowHeadRight, arrowColor);
                            drawList->AddTriangle(arrowEnd, arrowHeadLeft, arrowHeadRight, arrowOutlineColor, 1.0f);
                        }
                    }
                }
            }

            drawList->PopClipRect();

            if (hasSelectedEntity) {
                SceneEntity* selectedEntity = m_activeScene.getEntityByIndex(static_cast<std::size_t>(m_selectedEntityIndex));
                if (selectedEntity != nullptr) {
                    const glm::mat4 selectedWorld = composeEntityWorldTransformMatrix(currentEntities, static_cast<std::size_t>(m_selectedEntityIndex));

                    float manipulatedMatrix[16] = {};
                    std::memcpy(manipulatedMatrix, glm::value_ptr(selectedWorld), sizeof(manipulatedMatrix));

                    ImGuizmo::SetDrawlist(drawList);
                    ImGuizmo::SetRect(
                        imageRectMin.x,
                        imageRectMin.y,
                        imageRectMax.x - imageRectMin.x,
                        imageRectMax.y - imageRectMin.y
                    );
                    ImGuizmo::SetOrthographic(false);
                    ImGuizmo::SetGizmoSizeClipSpace(std::clamp(m_gizmoSizeClipSpace, 0.06f, 0.30f));
                    const ImGuizmo::OPERATION operation = getGizmoOperationFromIndex(m_gizmoOperationIndex);

                    float translationSnap[3] = {
                        m_gizmoTranslateSnap,
                        m_gizmoTranslateSnap,
                        m_gizmoTranslateSnap
                    };
                    float scaleSnap[3] = {
                        m_gizmoScaleSnap,
                        m_gizmoScaleSnap,
                        m_gizmoScaleSnap
                    };
                    float rotateSnap = m_gizmoRotateSnapDegrees;

                    const float* snapValues = nullptr;
                    if (m_gizmoSnapEnabled) {
                        if (operation == ImGuizmo::ROTATE) {
                            snapValues = &rotateSnap;
                        } else if (operation == ImGuizmo::SCALE) {
                            snapValues = scaleSnap;
                        } else {
                            snapValues = translationSnap;
                        }
                    }

                    const bool manipulated = ImGuizmo::Manipulate(
                        glm::value_ptr(viewMatrix),
                        glm::value_ptr(projectionMatrix),
                        operation,
                        getGizmoMode(m_gizmoWorldMode),
                        manipulatedMatrix,
                        nullptr,
                        snapValues,
                        nullptr,
                        nullptr
                    );

                    if (manipulated) {
                        glm::mat4 localMatrix = glm::make_mat4(manipulatedMatrix);
                        if (selectedEntity->parentIndex >= 0 && selectedEntity->parentIndex < static_cast<int>(currentEntities.size())) {
                            const glm::mat4 parentWorld = composeEntityWorldTransformMatrix(currentEntities, static_cast<std::size_t>(selectedEntity->parentIndex));
                            localMatrix = glm::inverse(parentWorld) * localMatrix;
                        }

                        applyMatrixToSceneTransform(localMatrix, selectedEntity->transform, operation, m_gizmoRotationSensitivity);
                    }
                }
            }
        }

        if (imageClicked && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            const float imageRectWidth = imageRectMax.x - imageRectMin.x;
            const float imageRectHeight = imageRectMax.y - imageRectMin.y;
            if (imageRectWidth > 1.0f && imageRectHeight > 1.0f) {
                const ImVec2 mousePosition = ImGui::GetIO().MousePos;

                int iconPickedEntityIndex = -1;
                float iconPickedDistance = std::numeric_limits<float>::max();
                for (const OverlayIconHitZone& iconZone : overlayIconHitZones) {
                    if (mousePosition.x < iconZone.min.x || mousePosition.x > iconZone.max.x ||
                        mousePosition.y < iconZone.min.y || mousePosition.y > iconZone.max.y) {
                        continue;
                    }

                    const float centerX = (iconZone.min.x + iconZone.max.x) * 0.5f;
                    const float centerY = (iconZone.min.y + iconZone.max.y) * 0.5f;
                    const float dx = mousePosition.x - centerX;
                    const float dy = mousePosition.y - centerY;
                    const float distanceSquared = dx * dx + dy * dy;
                    if (distanceSquared < iconPickedDistance) {
                        iconPickedDistance = distanceSquared;
                        iconPickedEntityIndex = iconZone.entityIndex;
                    }
                }

                if (iconPickedEntityIndex >= 0) {
                    setSelectedEntity(iconPickedEntityIndex);
                    ImGui::End();
                    return;
                }

                const float ndcX = ((mousePosition.x - imageRectMin.x) / imageRectWidth) * 2.0f - 1.0f;
                const float ndcY = 1.0f - ((mousePosition.y - imageRectMin.y) / imageRectHeight) * 2.0f;

                const glm::mat4 viewProjection = Renderer::getCameraProjectionMatrix() * Renderer::getCameraViewMatrix();
                const glm::mat4 inverseViewProjection = glm::inverse(viewProjection);

                glm::vec4 nearPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
                glm::vec4 farPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);

                if (std::abs(nearPoint.w) > 0.0001f && std::abs(farPoint.w) > 0.0001f) {
                    nearPoint /= nearPoint.w;
                    farPoint /= farPoint.w;

                    const glm::vec3 rayOrigin = glm::vec3(nearPoint);
                    const glm::vec3 rayDirection = glm::normalize(glm::vec3(farPoint - nearPoint));

                    const auto& currentEntities = m_activeScene.getEntityData();
                    int pickedEntityIndex = -1;
                    float nearestDistance = std::numeric_limits<float>::max();

                    for (std::size_t entityIndex = 0; entityIndex < currentEntities.size(); ++entityIndex) {
                        const SceneEntity& entity = currentEntities[entityIndex];
                        if (isCameraEntity(entity) || isLightEntity(entity) || entity.modelAssetName.empty()) {
                            continue;
                        }

                        const std::shared_ptr<Model> model = AssetLoader::getModel(entity.modelAssetName);
                        if (model == nullptr || !model->isLoaded()) {
                            continue;
                        }

                        glm::vec3 localBoundsMin;
                        glm::vec3 localBoundsMax;
                        if (!model->getLocalBounds(
                                localBoundsMin,
                                localBoundsMax,
                                entity.modelMeshIndices,
                                entity.applyModelNodeTransforms
                            )) {
                            continue;
                        }

                        const glm::mat4 worldTransform = composeEntityWorldTransformMatrix(currentEntities, entityIndex);
                        const glm::mat4 inverseWorldTransform = glm::inverse(worldTransform);
                        const glm::vec3 localRayOrigin = glm::vec3(inverseWorldTransform * glm::vec4(rayOrigin, 1.0f));
                        glm::vec3 localRayDirection = glm::vec3(inverseWorldTransform * glm::vec4(rayDirection, 0.0f));
                        if (glm::length(localRayDirection) <= 0.0001f) {
                            continue;
                        }

                        localRayDirection = glm::normalize(localRayDirection);
                        const std::optional<float> localHitDistance = rayAabbIntersectionDistance(
                            localRayOrigin,
                            localRayDirection,
                            localBoundsMin,
                            localBoundsMax
                        );
                        if (!localHitDistance.has_value()) {
                            continue;
                        }

                        const glm::vec3 localHitPosition = localRayOrigin + localRayDirection * localHitDistance.value();
                        const glm::vec3 worldHitPosition = glm::vec3(worldTransform * glm::vec4(localHitPosition, 1.0f));
                        const float hitDistance = glm::length(worldHitPosition - rayOrigin);

                        if (hitDistance < nearestDistance) {
                            nearestDistance = hitDistance;
                            pickedEntityIndex = static_cast<int>(entityIndex);
                        }
                    }

                    if (pickedEntityIndex >= 0) {
                        setSelectedEntity(pickedEntityIndex);
                    } else {
                        clearEntitySelection();
                    }
                } else {
                    clearEntitySelection();
                }
            } else {
                clearEntitySelection();
            }
        }

        if (!m_runtimeEntityCameraActive) {
            updateSceneCameraController(m_sceneViewImageHovered);
        }

        ImGui::Spacing();
        if (m_runtimeEntityCameraActive) {
            ImGui::TextDisabled("Runtime camera entity drives view (Play mode).");
        } else {
            ImGui::TextDisabled("MMB: Orbit  |  Wheel: Zoom  |  Ctrl+RMB: Pan  |  RMB: Context Menu");
        }

        ImGui::End();
    }


}
