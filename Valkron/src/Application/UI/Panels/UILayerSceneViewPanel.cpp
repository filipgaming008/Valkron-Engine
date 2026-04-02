#include "Application/UI/Panels/UILayerPanelsInternal.hpp"

#define IMVIEWGUIZMO_IMPLEMENTATION
#define ImLengthSqr ImViewGuizmo_ImLengthSqr
#include "ImViewGuizmo.h"
#undef ImLengthSqr

#include "glm/gtx/quaternion.hpp"

namespace Valkron {

    void UILayer::drawSceneViewPanel(float deltaTime) {
        const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (!ImGui::Begin("Scene View", &m_showSceneViewPanel, windowFlags)) {
            ImGui::End();
            return;
        }

        drawWindowPanelGradient();

        const auto& entities = m_activeScene.getEntityData();
        const bool hasSelectedEntity = m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(entities.size());

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 2.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(19, 21, 24, 230));
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(130, 40, 40, 135));
            if (ImGui::BeginChild("SceneViewHeaderStrip", ImVec2(0.0f, 72.0f), true, ImGuiWindowFlags_NoScrollbar)) {
                const float frameMs = deltaTime * 1000.0f;
                const float fps = deltaTime > 0.0f ? (1.0f / deltaTime) : 0.0f;

                ImGui::TextUnformatted("Scene Renderer | Edit");
                ImGui::Separator();
                ImGui::Text("Frame %.2f ms  FPS %.1f  Viewport %dx%d", frameMs, fps, Renderer::getViewportWidth(), Renderer::getViewportHeight());
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);

            ImGui::Text("Selected Entity: %s", hasSelectedEntity ? entities[static_cast<std::size_t>(m_selectedEntityIndex)].name.c_str() : "None");

            ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(20, 22, 25, 220));
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(122, 36, 36, 120));
            if (ImGui::BeginChild("SceneViewControlStrip", ImVec2(0.0f, 42.0f), true, ImGuiWindowFlags_NoScrollbar)) {
                ImGui::SetNextItemWidth(190.0f);
                if (ImGui::BeginCombo("##SceneSelection", hasSelectedEntity ? entities[static_cast<std::size_t>(m_selectedEntityIndex)].name.c_str() : "None")) {
                    if (ImGui::Selectable("None", !hasSelectedEntity)) {
                        clearEntitySelection();
                    }

                    for (std::size_t i = 0; i < entities.size(); ++i) {
                        const bool selected = static_cast<int>(i) == m_selectedEntityIndex;
                        if (ImGui::Selectable(entities[i].name.c_str(), selected)) {
                            setSelectedEntity(static_cast<int>(i));
                        }

                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    ImGui::EndCombo();
                }

                ImGui::SameLine();
                if (ImGui::Button("Deselect##SceneViewSelection")) {
                    clearEntitySelection();
                }

                ImGui::SameLine();
                ImGui::Checkbox("Grid", &m_showSceneGrid);
                ImGui::SameLine();
                if (ImGui::RadioButton("T", m_gizmoOperationIndex == 0)) {
                    m_gizmoOperationIndex = 0;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("R", m_gizmoOperationIndex == 1)) {
                    m_gizmoOperationIndex = 1;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("S", m_gizmoOperationIndex == 2)) {
                    m_gizmoOperationIndex = 2;
                }
                ImGui::SameLine();
                ImGui::Checkbox("World", &m_gizmoWorldMode);
                ImGui::SameLine();
                ImGui::Checkbox("Snap", &m_gizmoSnapEnabled);
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);

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

        const unsigned int frameTextureID = Renderer::getFrameTextureID();
        const int renderWidth = Renderer::getViewportWidth();
        const int renderHeight = Renderer::getViewportHeight();

        ImGui::Text("Render Target: %dx%d", renderWidth, renderHeight);

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
        const bool imageClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        m_sceneViewImageHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);

        {
            ImViewGuizmo::BeginFrame();
            ImViewGuizmo::Style& viewGizmoStyle = ImViewGuizmo::GetStyle();
            viewGizmoStyle.scale = 0.50f;
            viewGizmoStyle.bigCircleRadius = 62.0f;
            viewGizmoStyle.lineWidth = 3.0f;
            viewGizmoStyle.circleRadius = 12.0f;
            viewGizmoStyle.toolButtonRadius = 18.0f;
            viewGizmoStyle.labelSize = 0.90f;

            const float gizmoInset = 94.0f;
            const ImVec2 gizmoPos(imageRectMax.x - gizmoInset, imageRectMin.y + 14.0f);
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

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kModelAssetDragPayloadType)) {
                if (payload->Data != nullptr && payload->DataSize > 0) {
                    const std::string modelName(static_cast<const char*>(payload->Data));
                    if (!modelName.empty()) {
                        std::shared_ptr<Model> model = AssetLoader::getModel(modelName);
                        if (model == nullptr || !model->isLoaded()) {
                            const auto& assets = m_activeScene.getAssets();
                            const auto assetIt = std::find_if(assets.begin(), assets.end(), [&modelName](const SceneAsset& asset) {
                                return asset.name == modelName && isModelSceneAsset(asset);
                            });

                            if (assetIt != assets.end()) {
                                AssetLoader::loadModel(modelName, assetIt->path);
                                model = AssetLoader::getModel(modelName);
                            }
                        }

                        if (model != nullptr && model->isLoaded()) {
                            const std::string entityName = m_activeScene.makeUniqueEntityName(deriveAssetBaseName(modelName, "Model"));
                            m_activeScene.addEntity(entityName, SceneEntityType::Generic);

                            const std::optional<std::size_t> entityIndex = m_activeScene.findEntityIndex(entityName);
                            if (entityIndex.has_value()) {
                                if (SceneEntity* entity = m_activeScene.getEntityByIndex(entityIndex.value()); entity != nullptr) {
                                    entity->modelAssetName = modelName;
                                    entity->transform.position = m_runtimeEntityCameraActive ? Renderer::getCameraTarget() : m_sceneCameraPivot;
                                    entity->transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
                                    entity->transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
                                    entity->transform.size = glm::vec3(1.0f, 1.0f, 1.0f);
                                }

                                setSelectedEntity(static_cast<int>(entityIndex.value()));
                            }

                            appendTerminalLine("Placed model entity " + entityName + " from asset " + modelName + ".");
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
                        const glm::mat4 worldTransform = composeEntityWorldTransformMatrix(currentEntities, entityIndex);
                        const glm::vec3 entityCenter = extractWorldPosition(worldTransform);

                        const SceneTransform& transform = currentEntities[entityIndex].transform;
                        const glm::vec3 extents(
                            std::max(0.05f, std::abs(transform.size.x * transform.scale.x)),
                            std::max(0.05f, std::abs(transform.size.y * transform.scale.y)),
                            std::max(0.05f, std::abs(transform.size.z * transform.scale.z))
                        );
                        const float sphereRadius = std::max(0.2f, 0.5f * std::max({extents.x, extents.y, extents.z}));

                        const std::optional<float> hitDistance = raySphereIntersectionDistance(rayOrigin, rayDirection, entityCenter, sphereRadius);
                        if (!hitDistance.has_value()) {
                            continue;
                        }

                        if (hitDistance.value() < nearestDistance) {
                            nearestDistance = hitDistance.value();
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
            ImGui::TextDisabled("MMB: Orbit  |  Wheel: Zoom  |  Ctrl+RMB: Pan  |  RMB: Options");
        }

        ImGui::End();
    }


}
