#include "Application/UI/Panels/UILayerPanelsInternal.hpp"

namespace Valkron {

    void UILayer::drawSceneHierarchyPanel() {
        if (!ImGui::Begin("Scene Hierarchy", &m_showSceneHierarchyPanel)) {
            ImGui::End();
            return;
        }

        drawWindowPanelGradient();

        const auto& entities = m_activeScene.getEntityData();
        if (m_selectedEntityIndex >= static_cast<int>(entities.size())) {
            clearEntitySelection();
        }

        ImGui::Text("Scene: %s", m_activeScene.getName().c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("| Hierarchy");
        ImGui::Text("Entities: %d", static_cast<int>(entities.size()));

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

            std::string treeLabel = std::string(getEntityIconToken(entity)) + " [" + getEntityCategoryToken(entity) + "] " + entity.name;
            if (hasChildren) {
                treeLabel += "  {" + std::to_string(childrenByParent[entityIndex].size()) + "}";
            }

            const bool nodeOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(entityIndex + 1)), flags, "%s", treeLabel.c_str());
            ImGui::PopStyleColor(3);

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
            if (parentIndex < 0 || parentIndex >= static_cast<int>(entities.size())) {
                drawEntityNode(i);
                drewAnyNode = true;
            }
        }

        if (!drewAnyNode && !entities.empty() && !hasSearchFilter) {
            for (std::size_t i = 0; i < entities.size(); ++i) {
                drawEntityNode(i);
            }
        }

        if (entities.empty()) {
            ImGui::TextDisabled("No entities in this scene.");
        } else if (hasSearchFilter && !drewAnyNode) {
            ImGui::TextDisabled("No entities matched your search.");
        }

        if (ImGui::BeginPopupContextWindow("SceneHierarchyWindowContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Add Empty Entity")) {
                const std::string entityName = m_activeScene.makeUniqueEntityName("Entity");
                m_activeScene.addEntity(entityName);
                appendTerminalLine("Created " + entityName + ".");
            }
            if (ImGui::MenuItem("Add Camera Entity")) {
                const std::string entityName = m_activeScene.makeUniqueEntityName("Camera");
                m_activeScene.addEntity(entityName, SceneEntityType::Camera);
                appendTerminalLine("Created camera entity " + entityName + ".");
            }
            if (ImGui::MenuItem("Add Light Entity")) {
                const std::string entityName = m_activeScene.makeUniqueEntityName("Light");
                m_activeScene.addEntity(entityName, SceneEntityType::Light);
                appendTerminalLine("Created light entity " + entityName + ".");
            }
            if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(entities.size()) && ImGui::MenuItem("Create Child From Selection")) {
                pendingCreateChildEntity = static_cast<std::size_t>(m_selectedEntityIndex);
            }
            ImGui::EndPopup();
        }

        ImGui::Separator();
        if (ImGui::Button("Add Empty Entity")) {
            const std::string entityName = m_activeScene.makeUniqueEntityName("Entity");
            m_activeScene.addEntity(entityName);
            appendTerminalLine("Created " + entityName + ".");
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear Selection")) {
            clearEntitySelection();
        }

        ImGui::SameLine();
        const bool hasSelection = m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(entities.size());
        if (!hasSelection) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Delete Selected") && hasSelection) {
            pendingDeleteEntity = static_cast<std::size_t>(m_selectedEntityIndex);
        }

        if (!hasSelection) {
            ImGui::EndDisabled();
        }

        const auto processDelete = [&](std::size_t entityIndex) {
            const auto& currentEntities = m_activeScene.getEntityData();
            if (entityIndex >= currentEntities.size()) {
                return;
            }

            const std::string removedEntityName = currentEntities[entityIndex].name;
            if (m_activeScene.removeEntity(removedEntityName)) {
                appendTerminalLine("Removed " + removedEntityName + ".");
            }

            if (m_selectedEntityIndex == static_cast<int>(entityIndex)) {
                clearEntitySelection();
                return;
            }

            if (m_selectedEntityIndex > static_cast<int>(entityIndex)) {
                --m_selectedEntityIndex;
            }

            const auto& updatedEntities = m_activeScene.getEntityData();
            if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(updatedEntities.size())) {
                setSelectedEntity(m_selectedEntityIndex);
            } else {
                clearEntitySelection();
            }
        };

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
                    }

                    setSelectedEntity(static_cast<int>(duplicatedIndex.value()));
                    appendTerminalLine("Duplicated entity " + sourceEntity.name + " as " + duplicatedName + ".");
                }
            }
        }

        if (pendingDeleteEntity.has_value()) {
            processDelete(pendingDeleteEntity.value());
        }

        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsAnyItemHovered()) {
            clearEntitySelection();
        }

        ImGui::End();
    }


}
