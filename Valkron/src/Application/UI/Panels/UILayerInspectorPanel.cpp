#include "Application/UI/Panels/UILayerPanelsInternal.hpp"

namespace Valkron {

    void UILayer::drawInspectorPanel() {
        if (!ImGui::Begin("Inspector", &m_showInspectorPanel)) {
            ImGui::End();
            return;
        }

        drawWindowPanelGradient();

        const auto& entities = m_activeScene.getEntityData();
        const bool hasSelection = m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(entities.size());

        ImGui::Text("Scene Element Inspector");
        ImGui::Separator();
        ImGui::Text("Scene: %s", m_activeScene.getName().c_str());
        ImGui::Text("State: %s", sceneStateToString(m_activeScene.getState()));
        ImGui::Text("Entities: %d", static_cast<int>(entities.size()));
        ImGui::Text("Assets: %d", static_cast<int>(m_activeScene.getAssets().size()));

        if (!hasSelection) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("No selected entity.");
            ImGui::TextWrapped("Select an entity from Scene Hierarchy to edit transform, parent, and metadata.");
            ImGui::End();
            return;
        }

        SceneEntity* selectedEntity = m_activeScene.getEntityByIndex(static_cast<std::size_t>(m_selectedEntityIndex));
        if (selectedEntity == nullptr) {
            ImGui::End();
            return;
        }

        if (m_selectedEntityNameBufferEntityIndex != m_selectedEntityIndex) {
            std::fill(m_selectedEntityNameBuffer.begin(), m_selectedEntityNameBuffer.end(), '\0');
            std::snprintf(m_selectedEntityNameBuffer.data(), m_selectedEntityNameBuffer.size(), "%s", selectedEntity->name.c_str());
            m_selectedEntityNameBufferEntityIndex = m_selectedEntityIndex;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Entity");

        ImGui::InputText("Name", m_selectedEntityNameBuffer.data(), m_selectedEntityNameBuffer.size());
        ImGui::SameLine();
        if (ImGui::Button("Apply Name")) {
            std::string desiredName = std::string(m_selectedEntityNameBuffer.data());
            if (desiredName.empty()) {
                desiredName = "Entity";
            }

            if (desiredName != selectedEntity->name) {
                std::string appliedName = desiredName;
                if (m_activeScene.findEntityIndex(desiredName).has_value()) {
                    appliedName = m_activeScene.makeUniqueEntityName(desiredName);
                }

                const std::string oldName = selectedEntity->name;
                if (m_activeScene.renameEntity(oldName, appliedName)) {
                    appendTerminalLine("Renamed entity " + oldName + " to " + appliedName + ".");
                    m_activeScene.setGameStateValue("SelectedEntity", appliedName);
                    std::fill(m_selectedEntityNameBuffer.begin(), m_selectedEntityNameBuffer.end(), '\0');
                    std::snprintf(m_selectedEntityNameBuffer.data(), m_selectedEntityNameBuffer.size(), "%s", appliedName.c_str());
                }
            }
        }

        const auto& refreshedEntities = m_activeScene.getEntityData();
        if (m_selectedEntityIndex < 0 || m_selectedEntityIndex >= static_cast<int>(refreshedEntities.size())) {
            clearEntitySelection();
            ImGui::End();
            return;
        }

        selectedEntity = m_activeScene.getEntityByIndex(static_cast<std::size_t>(m_selectedEntityIndex));
        if (selectedEntity == nullptr) {
            ImGui::End();
            return;
        }

        const char* typePreview = getSceneEntityTypeDisplayName(selectedEntity->type);
        if (ImGui::BeginCombo("Type", typePreview)) {
            const std::array<SceneEntityType, 3> typeOptions = {
                SceneEntityType::Generic,
                SceneEntityType::Camera,
                SceneEntityType::Light
            };

            for (const SceneEntityType typeOption : typeOptions) {
                const bool selectedType = selectedEntity->type == typeOption;
                const char* typeLabel = getSceneEntityTypeDisplayName(typeOption);
                if (ImGui::Selectable(typeLabel, selectedType)) {
                    selectedEntity->type = typeOption;
                    if (selectedEntity->type != SceneEntityType::Generic && !selectedEntity->modelAssetName.empty()) {
                        selectedEntity->modelAssetName.clear();
                    }
                    appendTerminalLine("Set entity type for " + selectedEntity->name + " to " + std::string(typeLabel) + ".");
                }

                if (selectedType) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        const bool supportsModelBinding = selectedEntity->type == SceneEntityType::Generic;
        const char* modelBindingPreview = selectedEntity->modelAssetName.empty() ? "None" : selectedEntity->modelAssetName.c_str();
        if (!supportsModelBinding) {
            ImGui::BeginDisabled();
        }

        if (ImGui::BeginCombo("Model", modelBindingPreview)) {
            const bool hasNoModel = selectedEntity->modelAssetName.empty();
            if (ImGui::Selectable("None", hasNoModel)) {
                selectedEntity->modelAssetName.clear();
            }

            const std::vector<std::string> modelNames = AssetLoader::getModelNames();
            for (const std::string& modelName : modelNames) {
                const bool currentlySelectedModel = selectedEntity->modelAssetName == modelName;
                if (ImGui::Selectable(modelName.c_str(), currentlySelectedModel)) {
                    selectedEntity->modelAssetName = modelName;
                    appendTerminalLine("Assigned model " + modelName + " to entity " + selectedEntity->name + ".");
                }

                if (currentlySelectedModel) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        if (!supportsModelBinding) {
            ImGui::EndDisabled();
            ImGui::TextDisabled("Model binding is available for Generic entities only.");
        }

        int childCount = 0;
        for (const SceneEntity& entity : refreshedEntities) {
            if (entity.parentIndex == m_selectedEntityIndex) {
                ++childCount;
            }
        }

        int hierarchyDepth = 0;
        int currentParent = selectedEntity->parentIndex;
        while (currentParent >= 0 && currentParent < static_cast<int>(refreshedEntities.size())) {
            ++hierarchyDepth;
            currentParent = refreshedEntities[static_cast<std::size_t>(currentParent)].parentIndex;
        }

        ImGui::Text("Attributes");
        ImGui::BulletText("Entity ID: %d", m_selectedEntityIndex);
        ImGui::BulletText("Category: %s", getEntityCategoryToken(*selectedEntity));
        ImGui::BulletText("Type: %s", getSceneEntityTypeDisplayName(selectedEntity->type));
        ImGui::BulletText("Model: %s", selectedEntity->modelAssetName.empty() ? "None" : selectedEntity->modelAssetName.c_str());
        ImGui::BulletText("Hierarchy Depth: %d", hierarchyDepth);
        ImGui::BulletText("Child Count: %d", childCount);
        ImGui::BulletText("Root Entity: %s", selectedEntity->parentIndex < 0 ? "Yes" : "No");

        if (selectedEntity->parentIndex >= 0 && selectedEntity->parentIndex < static_cast<int>(refreshedEntities.size())) {
            ImGui::BulletText("Parent Entity: %s", refreshedEntities[static_cast<std::size_t>(selectedEntity->parentIndex)].name.c_str());
        } else {
            ImGui::BulletText("Parent Entity: None");
        }

        ImGui::Spacing();
        ImGui::Separator();

        const int currentParentIndex = selectedEntity->parentIndex;
        const char* parentPreview = "None";
        if (currentParentIndex >= 0 && currentParentIndex < static_cast<int>(refreshedEntities.size())) {
            parentPreview = refreshedEntities[static_cast<std::size_t>(currentParentIndex)].name.c_str();
        }

        if (ImGui::BeginCombo("Parent", parentPreview)) {
            const bool noParentSelected = currentParentIndex < 0;
            if (ImGui::Selectable("None", noParentSelected)) {
                if (m_activeScene.setEntityParent(static_cast<std::size_t>(m_selectedEntityIndex), std::nullopt)) {
                    appendTerminalLine("Parent cleared for " + selectedEntity->name + ".");
                }
            }

            for (std::size_t i = 0; i < refreshedEntities.size(); ++i) {
                if (static_cast<int>(i) == m_selectedEntityIndex) {
                    continue;
                }

                const bool isSelected = currentParentIndex == static_cast<int>(i);
                if (ImGui::Selectable(refreshedEntities[i].name.c_str(), isSelected)) {
                    if (m_activeScene.setEntityParent(static_cast<std::size_t>(m_selectedEntityIndex), i)) {
                        appendTerminalLine("Parent of " + selectedEntity->name + " set to " + refreshedEntities[i].name + ".");
                    } else {
                        appendTerminalLine("Parent assignment rejected for " + selectedEntity->name + " (cycle or invalid relationship).");
                    }
                }

                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Transform");

        bool transformChanged = false;
        transformChanged |= drawColoredVec3Control("Position", selectedEntity->transform.position, 0.05f, false, 0.0f, 0.0f);
        transformChanged |= drawColoredVec3Control("Rotation", selectedEntity->transform.rotation, 0.4f, false, 0.0f, 0.0f);
        transformChanged |= drawColoredVec3Control("Scale", selectedEntity->transform.scale, 0.02f, true, 0.01f, 500.0f);
        transformChanged |= drawColoredVec3Control("Size", selectedEntity->transform.size, 0.02f, true, 0.01f, 500.0f);

        selectedEntity->transform.scale.x = std::max(0.01f, selectedEntity->transform.scale.x);
        selectedEntity->transform.scale.y = std::max(0.01f, selectedEntity->transform.scale.y);
        selectedEntity->transform.scale.z = std::max(0.01f, selectedEntity->transform.scale.z);
        selectedEntity->transform.size.x = std::max(0.01f, selectedEntity->transform.size.x);
        selectedEntity->transform.size.y = std::max(0.01f, selectedEntity->transform.size.y);
        selectedEntity->transform.size.z = std::max(0.01f, selectedEntity->transform.size.z);

        if (transformChanged) {
            setSelectedEntity(m_selectedEntityIndex);
        }

        if (ImGui::Button("Reset Transform")) {
            selectedEntity->transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
            selectedEntity->transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
            selectedEntity->transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
            selectedEntity->transform.size = glm::vec3(1.0f, 1.0f, 1.0f);
            appendTerminalLine("Transform reset for " + selectedEntity->name + ".");
        }

        ImGui::End();
    }


}
