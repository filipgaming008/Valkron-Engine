#include "Application/UI/Panels/UILayerPanelsInternal.hpp"

namespace Valkron {

    void UILayer::drawSceneHierarchyPanel() {
        if (!ImGui::Begin("Scene Hierarchy", &m_showSceneHierarchyPanel)) {
            ImGui::End();
            return;
        }

        drawWindowPanelGradient();

        auto trimSceneName = [](std::string value) {
            value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
            value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(), value.end());
            return value;
        };

        auto makeUniqueSceneName = [&](const std::string& baseName, std::optional<std::size_t> ignoreSceneIndex) {
            std::string resolvedBaseName = trimSceneName(baseName);
            if (resolvedBaseName.empty()) {
                resolvedBaseName = "Scene";
            }

            auto nameExists = [&](const std::string& candidateName) {
                for (std::size_t i = 0; i < m_editorScenes.size(); ++i) {
                    if (ignoreSceneIndex.has_value() && ignoreSceneIndex.value() == i) {
                        continue;
                    }
                    if (m_editorScenes[i].getName() == candidateName) {
                        return true;
                    }
                }
                return false;
            };

            if (!nameExists(resolvedBaseName)) {
                return resolvedBaseName;
            }

            for (int suffix = 1;; ++suffix) {
                const std::string candidate = resolvedBaseName + "_" + std::to_string(suffix);
                if (!nameExists(candidate)) {
                    return candidate;
                }
            }
        };

        auto createSceneWithDefaults = [](const std::string& sceneName) {
            Scene scene(sceneName);

            scene.addEntity("Camera", SceneEntityType::Camera);
            scene.addEntity("Directional Light", SceneEntityType::Light);
            scene.addEntity("Cube", SceneEntityType::Generic);

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

            if (const std::optional<std::size_t> cubeEntityIndex = scene.findEntityIndex("Cube"); cubeEntityIndex.has_value()) {
                if (SceneEntity* cubeEntity = scene.getEntityByIndex(cubeEntityIndex.value()); cubeEntity != nullptr) {
                    cubeEntity->modelAssetName = "test_cube.obj";
                    cubeEntity->modelMeshIndices.clear();
                    cubeEntity->applyModelNodeTransforms = true;
                    ensureEntityUsesPbrComponent(*cubeEntity);
                }
            }

            scene.addAsset("checker.ppm", "assets/textures/checker.ppm");
            scene.addAsset("textured.vert", "assets/shaders/textured.vert");
            scene.addAsset("textured.frag", "assets/shaders/textured.frag");
            scene.addAsset("blinn_phong.vert", "assets/shaders/blinn_phong.vert");
            scene.addAsset("blinn_phong.frag", "assets/shaders/blinn_phong.frag");
            scene.addAsset("pbr.vert", "assets/shaders/pbr.vert");
            scene.addAsset("pbr.frag", "assets/shaders/pbr.frag");
            scene.addAsset("default_compute.comp", "assets/shaders/default_compute.comp");
            scene.addAsset("test_cube.obj", "assets/models/test_cube.obj");

            scene.setGameStateValue("Mode", "Edit");
            scene.setGameStateValue("SelectedEntity", "None");
            scene.setGameStateValue("PrimaryCameraEntity", "Camera");
            return scene;
        };

        enum class SceneNameDialogMode {
            Create,
            Rename
        };

        static SceneNameDialogMode sceneNameDialogMode = SceneNameDialogMode::Create;
        static std::array<char, 128> sceneNameDialogBuffer{};

        std::optional<std::size_t> pendingSceneSwitch;
        std::optional<SceneEntityType> pendingCreateTopLevelEntity;
        bool requestSceneNameDialogOpen = false;
        bool pendingDeleteActiveScene = false;

        ImGui::TextUnformatted("Scene");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-100.0f);
        if (ImGui::BeginCombo("##HierarchySceneSelection", m_activeScene.getName().c_str())) {
            for (std::size_t sceneIndex = 0; sceneIndex < m_editorScenes.size(); ++sceneIndex) {
                const bool selected = static_cast<int>(sceneIndex) == m_activeSceneLibraryIndex;
                const std::string& sceneName = m_editorScenes[sceneIndex].getName();
                if (ImGui::Selectable(sceneName.c_str(), selected)) {
                    pendingSceneSwitch = sceneIndex;
                }

                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::Button("Scenes")) {
            ImGui::OpenPopup("SceneHierarchy.SceneActionsPopup");
        }

        if (ImGui::BeginPopup("SceneHierarchy.SceneActionsPopup")) {
            if (ImGui::MenuItem("Create Scene")) {
                sceneNameDialogMode = SceneNameDialogMode::Create;
                const std::string suggestedName = makeUniqueSceneName("New Scene", std::nullopt);
                std::fill(sceneNameDialogBuffer.begin(), sceneNameDialogBuffer.end(), '\0');
                std::snprintf(sceneNameDialogBuffer.data(), sceneNameDialogBuffer.size(), "%s", suggestedName.c_str());
                requestSceneNameDialogOpen = true;
            }

            if (ImGui::MenuItem("Rename Scene")) {
                sceneNameDialogMode = SceneNameDialogMode::Rename;
                std::fill(sceneNameDialogBuffer.begin(), sceneNameDialogBuffer.end(), '\0');
                std::snprintf(sceneNameDialogBuffer.data(), sceneNameDialogBuffer.size(), "%s", m_activeScene.getName().c_str());
                requestSceneNameDialogOpen = true;
            }

            const bool canDeleteScene = m_editorScenes.size() > 1;
            if (!canDeleteScene) {
                ImGui::BeginDisabled();
            }
            if (ImGui::MenuItem("Delete Scene")) {
                pendingDeleteActiveScene = true;
            }
            if (!canDeleteScene) {
                ImGui::EndDisabled();
                ImGui::TextDisabled("At least one scene must remain.");
            }

            ImGui::EndPopup();
        }

        if (pendingSceneSwitch.has_value()) {
            switchToSceneByIndex(pendingSceneSwitch.value());
        }

        if (pendingDeleteActiveScene) {
            if (m_editorScenes.size() <= 1) {
                appendTerminalLine("Cannot delete the last remaining scene.");
            } else {
                syncActiveSceneToLibrary();

                std::size_t eraseIndex = 0;
                if (m_activeSceneLibraryIndex >= 0) {
                    eraseIndex = std::min<std::size_t>(static_cast<std::size_t>(m_activeSceneLibraryIndex), m_editorScenes.size() - 1);
                }

                const std::string erasedSceneName = m_editorScenes[eraseIndex].getName();
                m_editorScenes.erase(m_editorScenes.begin() + static_cast<std::ptrdiff_t>(eraseIndex));

                const std::size_t newSceneIndex = std::min<std::size_t>(eraseIndex, m_editorScenes.size() - 1);
                m_activeSceneLibraryIndex = static_cast<int>(newSceneIndex);
                m_activeScene = m_editorScenes[newSceneIndex];
                clearEntitySelection();
                m_sceneCameraControllerInitialized = false;

                appendTerminalLine("Deleted scene " + erasedSceneName + ".");
                appendTerminalLine("Switched to scene " + m_activeScene.getName() + ".");
            }
        }

        if (requestSceneNameDialogOpen) {
            ImGui::OpenPopup("SceneHierarchy.SceneNameDialog");
        }

        if (ImGui::BeginPopupModal("SceneHierarchy.SceneNameDialog", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            const bool createMode = sceneNameDialogMode == SceneNameDialogMode::Create;
            ImGui::TextUnformatted(createMode ? "Create Scene" : "Rename Scene");
            ImGui::Separator();
            ImGui::SetNextItemWidth(260.0f);
            ImGui::InputText("Scene Name", sceneNameDialogBuffer.data(), sceneNameDialogBuffer.size(), ImGuiInputTextFlags_AutoSelectAll);

            if (ImGui::Button(createMode ? "Create" : "Rename", ImVec2(110.0f, 0.0f))) {
                const std::string enteredName = trimSceneName(std::string(sceneNameDialogBuffer.data()));
                const std::string resolvedName = makeUniqueSceneName(enteredName.empty() ? "Scene" : enteredName, createMode ? std::nullopt : std::optional<std::size_t>(static_cast<std::size_t>(std::max(0, m_activeSceneLibraryIndex))));

                if (createMode) {
                    syncActiveSceneToLibrary();
                    m_editorScenes.push_back(createSceneWithDefaults(resolvedName));
                    switchToSceneByIndex(m_editorScenes.size() - 1);
                    appendTerminalLine("Created scene " + resolvedName + ".");
                } else {
                    const std::string previousSceneName = m_activeScene.getName();
                    m_activeScene.setName(resolvedName);
                    syncActiveSceneToLibrary();
                    appendTerminalLine("Renamed scene " + previousSceneName + " to " + resolvedName + ".");
                }

                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(110.0f, 0.0f))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        const auto& entities = m_activeScene.getEntityData();
        if (m_selectedEntityIndex >= static_cast<int>(entities.size())) {
            clearEntitySelection();
        }

        const std::string primaryPlayCameraName = m_activeScene.getGameStateValue("PrimaryCameraEntity").value_or("");

        ImGui::TextDisabled("Entities: %d", static_cast<int>(entities.size()));

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##HierarchySearch", "Search entities...", m_hierarchySearchBuffer.data(), m_hierarchySearchBuffer.size());
        const std::string searchFilter = toLowercase(std::string(m_hierarchySearchBuffer.data()));
        const bool hasSearchFilter = !searchFilter.empty();
        ImGui::Separator();

        std::vector<std::vector<std::size_t>> childrenByParent(entities.size());
        for (std::size_t i = 0; i < entities.size(); ++i) {
            const int parentIndex = entities[i].parentIndex;
            if (parentIndex >= 0 && parentIndex < static_cast<int>(entities.size())) {
                childrenByParent[static_cast<std::size_t>(parentIndex)].push_back(i);
            }
        }

        std::optional<std::size_t> pendingDeleteEntity;
        std::optional<std::size_t> pendingDuplicateEntity;
        std::optional<std::size_t> pendingCreateChildEntity;
        std::optional<std::pair<std::size_t, std::size_t>> pendingReparentEntity;
        std::optional<std::size_t> pendingClearParentByDrop;

        ImGui::TextDisabled("Drag entities here to place them at scene root");
        const bool rootSelected = m_selectedEntityIndex < 0;
        if (ImGui::Selectable("Scene Root", rootSelected, ImGuiSelectableFlags_SpanAvailWidth)) {
            clearEntitySelection();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SceneHierarchy.EntityIndex")) {
                if (payload->DataSize == sizeof(std::size_t)) {
                    pendingClearParentByDrop = *static_cast<const std::size_t*>(payload->Data);
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::Separator();

        std::vector<std::string> lowerEntityNames;
        lowerEntityNames.reserve(entities.size());
        for (const SceneEntity& entity : entities) {
            lowerEntityNames.push_back(toLowercase(entity.name));
        }

        std::function<bool(std::size_t)> subtreeMatchesFilter = [&](std::size_t entityIndex) {
            if (entityIndex >= entities.size()) {
                return false;
            }

            if (!hasSearchFilter) {
                return true;
            }

            if (lowerEntityNames[entityIndex].find(searchFilter) != std::string::npos) {
                return true;
            }

            for (std::size_t childIndex : childrenByParent[entityIndex]) {
                if (subtreeMatchesFilter(childIndex)) {
                    return true;
                }
            }

            return false;
        };

        std::function<void(std::size_t)> drawEntityNode = [&](std::size_t entityIndex) {
            if (entityIndex >= entities.size() || !subtreeMatchesFilter(entityIndex)) {
                return;
            }

            const SceneEntity& entity = entities[entityIndex];
            const bool hasChildren = !childrenByParent[entityIndex].empty();
            const bool selected = m_selectedEntityIndex == static_cast<int>(entityIndex);

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
            if (!hasChildren) {
                flags |= ImGuiTreeNodeFlags_Leaf;
            }
            if (selected) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }

            ImVec4 badgeColor = getEntityBadgeColor(entity);
            if (selected) {
                badgeColor = brightenColor(badgeColor, 0.12f);
            }

            ImGui::PushStyleColor(ImGuiCol_Header, badgeColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, brightenColor(badgeColor, 0.08f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, brightenColor(badgeColor, 0.14f));

            std::string treeLabel = entity.name;
            if (hasChildren) {
                treeLabel += "  (" + std::to_string(childrenByParent[entityIndex].size()) + ")";
            }

            const bool nodeOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(entityIndex + 1)), flags, "%s", treeLabel.c_str());
            ImGui::PopStyleColor(3);

            const char* typeLabel = getSceneEntityTypeDisplayName(entity.type);
            const ImVec2 itemMin = ImGui::GetItemRectMin();
            const ImVec2 itemMax = ImGui::GetItemRectMax();
            const ImVec2 typeTextSize = ImGui::CalcTextSize(typeLabel);
            const float chipWidth = typeTextSize.x + 12.0f;
            const float chipHeight = typeTextSize.y + 2.0f;
            const float rightEdge = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
            const float chipX = rightEdge - chipWidth - 8.0f;

            if (chipX > itemMax.x + 8.0f) {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImU32 chipColor = ImGui::GetColorU32(brightenColor(badgeColor, -0.05f));
                const ImU32 chipBorderColor = ImGui::GetColorU32(brightenColor(badgeColor, 0.12f));

                const ImVec2 chipMin(chipX, itemMin.y + 1.0f);
                const ImVec2 chipMax(chipX + chipWidth, chipMin.y + chipHeight);
                drawList->AddRectFilled(chipMin, chipMax, chipColor, 5.0f);
                drawList->AddRect(chipMin, chipMax, chipBorderColor, 5.0f, 0, 1.0f);
                drawList->AddText(ImVec2(chipMin.x + 6.0f, chipMin.y + 1.0f), IM_COL32(225, 228, 236, 255), typeLabel);
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                setSelectedEntity(static_cast<int>(entityIndex));
            }

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                const std::size_t payloadEntityIndex = entityIndex;
                ImGui::SetDragDropPayload("SceneHierarchy.EntityIndex", &payloadEntityIndex, sizeof(payloadEntityIndex));
                ImGui::Text("Reparent %s", entity.name.c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SceneHierarchy.EntityIndex")) {
                    if (payload->DataSize == sizeof(std::size_t)) {
                        const std::size_t sourceEntityIndex = *static_cast<const std::size_t*>(payload->Data);
                        if (sourceEntityIndex != entityIndex) {
                            pendingReparentEntity = std::make_pair(sourceEntityIndex, entityIndex);
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Select")) {
                    setSelectedEntity(static_cast<int>(entityIndex));
                }
                if (ImGui::MenuItem("Create Child Entity")) {
                    pendingCreateChildEntity = entityIndex;
                }
                if (ImGui::MenuItem("Duplicate Entity")) {
                    pendingDuplicateEntity = entityIndex;
                }
                if (entity.parentIndex >= 0 && ImGui::MenuItem("Clear Parent")) {
                    if (m_activeScene.setEntityParent(entityIndex, std::nullopt)) {
                        appendTerminalLine("Removed parent from " + entity.name + ".");
                    }
                }
                if (entity.type == SceneEntityType::Camera) {
                    const bool isMainPlayCamera = primaryPlayCameraName == entity.name;
                    if (ImGui::MenuItem("Set As Main Play Camera", nullptr, isMainPlayCamera, !isMainPlayCamera)) {
                        m_activeScene.setGameStateValue("PrimaryCameraEntity", entity.name);
                        appendTerminalLine(entity.name + " is now the primary play camera.");
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete Entity")) {
                    pendingDeleteEntity = entityIndex;
                }
                ImGui::EndPopup();
            }

            if (nodeOpen) {
                for (std::size_t childIndex : childrenByParent[entityIndex]) {
                    drawEntityNode(childIndex);
                }
                ImGui::TreePop();
            }
        };

        bool drewAnyNode = false;
        for (std::size_t i = 0; i < entities.size(); ++i) {
            const int parentIndex = entities[i].parentIndex;
            if ((parentIndex < 0 || parentIndex >= static_cast<int>(entities.size())) && subtreeMatchesFilter(i)) {
                drawEntityNode(i);
                drewAnyNode = true;
            }
        }

        if (!drewAnyNode && !entities.empty() && !hasSearchFilter) {
            for (std::size_t i = 0; i < entities.size(); ++i) {
                drawEntityNode(i);
            }
            drewAnyNode = true;
        }

        if (entities.empty()) {
            ImGui::TextDisabled("No entities in this scene.");
        } else if (hasSearchFilter && !drewAnyNode) {
            ImGui::TextDisabled("No entities matched your search.");
        }

        if (ImGui::BeginPopupContextWindow("SceneHierarchy.BackgroundContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            ImGui::TextDisabled("Add Entity");
            if (ImGui::MenuItem("Empty Entity")) {
                pendingCreateTopLevelEntity = SceneEntityType::Generic;
            }
            if (ImGui::MenuItem("Camera Entity")) {
                pendingCreateTopLevelEntity = SceneEntityType::Camera;
            }
            if (ImGui::MenuItem("Light Entity")) {
                pendingCreateTopLevelEntity = SceneEntityType::Light;
            }
            ImGui::EndPopup();
        }

        if (pendingClearParentByDrop.has_value()) {
            const auto& currentEntities = m_activeScene.getEntityData();
            if (pendingClearParentByDrop.value() < currentEntities.size()) {
                const std::string movedEntityName = currentEntities[pendingClearParentByDrop.value()].name;
                if (m_activeScene.setEntityParent(pendingClearParentByDrop.value(), std::nullopt)) {
                    appendTerminalLine("Moved " + movedEntityName + " to root.");
                }
            }
        }

        if (pendingReparentEntity.has_value()) {
            const std::size_t sourceEntityIndex = pendingReparentEntity->first;
            const std::size_t targetParentIndex = pendingReparentEntity->second;

            const auto& currentEntities = m_activeScene.getEntityData();
            if (sourceEntityIndex < currentEntities.size() && targetParentIndex < currentEntities.size()) {
                const std::string sourceName = currentEntities[sourceEntityIndex].name;
                const std::string parentName = currentEntities[targetParentIndex].name;

                if (m_activeScene.setEntityParent(sourceEntityIndex, targetParentIndex)) {
                    appendTerminalLine("Reparented " + sourceName + " under " + parentName + ".");
                } else {
                    appendTerminalLine("Unable to reparent " + sourceName + " under " + parentName + ".");
                }
            }
        }

        if (pendingCreateChildEntity.has_value()) {
            const auto& currentEntities = m_activeScene.getEntityData();
            if (pendingCreateChildEntity.value() < currentEntities.size()) {
                const std::string parentName = currentEntities[pendingCreateChildEntity.value()].name;
                const std::string childName = m_activeScene.makeUniqueEntityName(parentName + "_Child");
                m_activeScene.addEntity(childName);

                const std::optional<std::size_t> childIndex = m_activeScene.findEntityIndex(childName);
                if (childIndex.has_value() && m_activeScene.setEntityParent(childIndex.value(), pendingCreateChildEntity.value())) {
                    setSelectedEntity(static_cast<int>(childIndex.value()));
                    appendTerminalLine("Created child entity " + childName + " under " + parentName + ".");
                }
            }
        }

        if (pendingDuplicateEntity.has_value()) {
            const auto& currentEntities = m_activeScene.getEntityData();
            if (pendingDuplicateEntity.value() < currentEntities.size()) {
                const SceneEntity sourceEntity = currentEntities[pendingDuplicateEntity.value()];
                const std::string duplicatedName = m_activeScene.makeUniqueEntityName(sourceEntity.name + "_Copy");
                m_activeScene.addEntity(duplicatedName, sourceEntity.type);

                const std::optional<std::size_t> duplicatedIndex = m_activeScene.findEntityIndex(duplicatedName);
                if (duplicatedIndex.has_value()) {
                    SceneEntity* duplicatedEntity = m_activeScene.getEntityByIndex(duplicatedIndex.value());
                    if (duplicatedEntity != nullptr) {
                        duplicatedEntity->transform = sourceEntity.transform;
                        duplicatedEntity->parentIndex = sourceEntity.parentIndex;
                        duplicatedEntity->type = sourceEntity.type;
                        duplicatedEntity->modelAssetName = sourceEntity.modelAssetName;
                        duplicatedEntity->modelMeshIndices = sourceEntity.modelMeshIndices;
                        duplicatedEntity->applyModelNodeTransforms = sourceEntity.applyModelNodeTransforms;
                        duplicatedEntity->shaderComponent = sourceEntity.shaderComponent;
                        duplicatedEntity->lightComponent = sourceEntity.lightComponent;
                    }

                    setSelectedEntity(static_cast<int>(duplicatedIndex.value()));
                    appendTerminalLine("Duplicated entity " + sourceEntity.name + " as " + duplicatedName + ".");
                }
            }
        }

        if (pendingDeleteEntity.has_value()) {
            (void)deleteEntityByIndex(pendingDeleteEntity.value());
        }

        if (pendingCreateTopLevelEntity.has_value()) {
            SceneEntityType entityType = pendingCreateTopLevelEntity.value();
            std::string baseEntityName = "Entity";
            if (entityType == SceneEntityType::Camera) {
                baseEntityName = "Camera";
            } else if (entityType == SceneEntityType::Light) {
                baseEntityName = "Light";
            }

            const std::string entityName = m_activeScene.makeUniqueEntityName(baseEntityName);
            m_activeScene.addEntity(entityName, entityType);

            if (entityType == SceneEntityType::Camera && m_activeScene.getGameStateValue("PrimaryCameraEntity").value_or("").empty()) {
                m_activeScene.setGameStateValue("PrimaryCameraEntity", entityName);
            }

            if (const std::optional<std::size_t> createdEntityIndex = m_activeScene.findEntityIndex(entityName); createdEntityIndex.has_value()) {
                setSelectedEntity(static_cast<int>(createdEntityIndex.value()));
            }

            appendTerminalLine("Created " + entityName + ".");
        }

        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsAnyItemHovered()) {
            clearEntitySelection();
        }

        ImGui::End();
    }


}
