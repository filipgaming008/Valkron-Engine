#include "Application/UI/Panels/UILayerPanelsInternal.hpp"

namespace Valkron {

    void UILayer::drawBottomPanel() {
        if (!ImGui::Begin("Project", &m_showBottomPanel)) {
            ImGui::End();
            return;
        }

        drawWindowPanelGradient();

        if (ImGui::BeginTabBar("ProjectTabs", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("Project")) {
                ImGui::Text("Asset Viewer / Loader");
                ImGui::Separator();
                drawAssetsPanel();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Console")) {
                drawTerminalPanel();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    void UILayer::drawAssetsPanel() {
        constexpr float kFixedAssetIconSize = 72.0f;

        ImGui::Text("Runtime Importer");
        if (ImGui::Button("Import Asset...")) {
            loadRuntimeAssetAuto();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("Auto-detects texture/model/shader/compute types and registers them in the asset list.");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Asset Viewer");

        const auto& assets = m_activeScene.getAssets();

        std::string currentFolder = normalizeFolderPath(m_assetBrowserFolderFilter);
        if (currentFolder == "All") {
            currentFolder.clear();
        }
        if (currentFolder != m_assetBrowserFolderFilter) {
            m_assetBrowserFolderFilter = currentFolder;
        }

        std::set<std::string> childFolders;
        std::vector<std::size_t> filteredAssetIndices;
        filteredAssetIndices.reserve(assets.size());

        for (std::size_t i = 0; i < assets.size(); ++i) {
            const std::string assetFolderPath = getAssetFolderPath(assets[i]);
            if (assetFolderPath == currentFolder) {
                filteredAssetIndices.push_back(i);
            }

            const std::optional<std::string> childFolder = getDirectChildFolder(currentFolder, assetFolderPath);
            if (childFolder.has_value()) {
                childFolders.insert(childFolder.value());
            }
        }

        if (m_selectedAssetIndex >= 0 && m_selectedAssetIndex < static_cast<int>(assets.size())) {
            const std::string selectedFolder = getAssetFolderPath(assets[static_cast<std::size_t>(m_selectedAssetIndex)]);
            const bool selectedAssetVisible = selectedFolder == currentFolder;
            if (!selectedAssetVisible) {
                m_selectedAssetIndex = -1;
            }
        }

        if (ImGui::Button("Root")) {
            currentFolder.clear();
            m_assetBrowserFolderFilter.clear();
        }

        ImGui::SameLine();
        const bool canGoUp = !currentFolder.empty();
        if (!canGoUp) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Up")) {
            currentFolder = getParentFolderPath(currentFolder);
            m_assetBrowserFolderFilter = currentFolder;
        }
        if (!canGoUp) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        ImGui::Text("Folder: %s", currentFolder.empty() ? "Root" : currentFolder.c_str());

        if (ImGui::BeginChild("AssetFolderNavigator", ImVec2(0.0f, 44.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
            if (childFolders.empty()) {
                ImGui::TextDisabled("No subfolders.");
            } else {
                bool firstFolderButton = true;
                for (const std::string& childFolderPath : childFolders) {
                    if (!firstFolderButton) {
                        ImGui::SameLine();
                    }
                    firstFolderButton = false;

                    const std::string childLabel = std::string("DIR ") + getFolderLeafName(childFolderPath);
                    if (ImGui::Button((childLabel + "##asset_folder_" + childFolderPath).c_str())) {
                        currentFolder = childFolderPath;
                        m_assetBrowserFolderFilter = childFolderPath;
                        m_selectedAssetIndex = -1;
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::Text("Icon Size: %.0f px (fixed)", kFixedAssetIconSize);
        ImGui::SameLine();
        ImGui::Text("Visible: %d / Total: %d", static_cast<int>(filteredAssetIndices.size()), static_cast<int>(assets.size()));

        if (m_selectedAssetIndex >= static_cast<int>(assets.size())) {
            m_selectedAssetIndex = -1;
        }

        if (filteredAssetIndices.empty()) {
            ImGui::TextDisabled("No assets in this folder.");
        } else {
            const float cellWidth = kFixedAssetIconSize + 30.0f;
            const int columns = std::max(1, static_cast<int>(std::max(1.0f, ImGui::GetContentRegionAvail().x) / cellWidth));

            if (ImGui::BeginTable("AssetIconGrid", columns, ImGuiTableFlags_SizingStretchSame)) {
                for (std::size_t assetIndex : filteredAssetIndices) {
                    ImGui::TableNextColumn();
                    ImGui::PushID(static_cast<int>(assetIndex));

                    const SceneAsset& asset = assets[assetIndex];
                    ImVec4 assetColor = getAssetIconColor(asset);
                    if (m_selectedAssetIndex == static_cast<int>(assetIndex)) {
                        assetColor = brightenColor(assetColor, 0.12f);
                    }

                    ImGui::PushStyleColor(ImGuiCol_Button, assetColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, brightenColor(assetColor, 0.08f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, brightenColor(assetColor, 0.14f));

                    const std::string iconLabel = std::string(getAssetIconToken(asset)) + "##asset_icon";
                    if (ImGui::Button(iconLabel.c_str(), ImVec2(kFixedAssetIconSize, kFixedAssetIconSize))) {
                        m_selectedAssetIndex = static_cast<int>(assetIndex);
                    }

                    if (isModelSceneAsset(asset) && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                        ImGui::SetDragDropPayload(kModelAssetDragPayloadType, asset.name.c_str(), asset.name.size() + 1);
                        ImGui::Text("Place model in Scene View");
                        ImGui::TextDisabled("%s", asset.name.c_str());
                        ImGui::EndDragDropSource();
                    }

                    const bool iconHovered = ImGui::IsItemHovered();
                    ImGui::PopStyleColor(3);

                    if (iconHovered) {
                        ImGui::BeginTooltip();
                        ImGui::Text("%s", asset.name.c_str());
                        ImGui::TextDisabled("%s", asset.path.c_str());
                        ImGui::EndTooltip();
                    }

                    ImGui::TextWrapped("%s", asset.name.c_str());
                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
        }

        if (m_selectedAssetIndex >= 0 && m_selectedAssetIndex < static_cast<int>(assets.size())) {
            const SceneAsset& selectedAsset = assets[static_cast<std::size_t>(m_selectedAssetIndex)];
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Selected Asset");
            ImGui::BulletText("Name: %s", selectedAsset.name.c_str());
            ImGui::BulletText("Folder: %s", getAssetFolderPath(selectedAsset).empty() ? "Root" : getAssetFolderPath(selectedAsset).c_str());
            ImGui::TextWrapped("Path: %s", selectedAsset.path.c_str());

            if (isModelSceneAsset(selectedAsset)) {
                if (ImGui::Button("Add Model Entity To Scene")) {
                    std::shared_ptr<Model> model = AssetLoader::getModel(selectedAsset.name);
                    if (model == nullptr || !model->isLoaded()) {
                        AssetLoader::loadModel(selectedAsset.name, selectedAsset.path);
                        model = AssetLoader::getModel(selectedAsset.name);
                    }

                    if (model != nullptr && model->isLoaded()) {
                        const std::string entityName = m_activeScene.makeUniqueEntityName(deriveAssetBaseName(selectedAsset.name, "Model"));
                        m_activeScene.addEntity(entityName, SceneEntityType::Generic);

                        if (const std::optional<std::size_t> entityIndex = m_activeScene.findEntityIndex(entityName); entityIndex.has_value()) {
                            if (SceneEntity* entity = m_activeScene.getEntityByIndex(entityIndex.value()); entity != nullptr) {
                                entity->modelAssetName = selectedAsset.name;
                                entity->transform.position = m_sceneCameraPivot;
                            }
                            setSelectedEntity(static_cast<int>(entityIndex.value()));
                        }

                        appendTerminalLine("Added model entity " + entityName + " from asset " + selectedAsset.name + ".");
                    } else {
                        appendTerminalLine("Unable to load model for scene placement: " + selectedAsset.name + ".");
                    }
                }

                ImGui::SameLine();
                ImGui::TextDisabled("(or drag icon into Scene View)");
            }

            if (ImGui::Button("Remove From Scene Asset List")) {
                int clearedBindings = 0;
                if (isModelSceneAsset(selectedAsset)) {
                    const auto& entities = m_activeScene.getEntityData();
                    for (std::size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex) {
                        if (SceneEntity* entity = m_activeScene.getEntityByIndex(entityIndex); entity != nullptr && entity->modelAssetName == selectedAsset.name) {
                            entity->modelAssetName.clear();
                            ++clearedBindings;
                        }
                    }
                }

                if (m_activeScene.removeAsset(selectedAsset.name)) {
                    if (clearedBindings > 0) {
                        appendTerminalLine(
                            "Removed scene asset entry: " + selectedAsset.name +
                            " (cleared " + std::to_string(clearedBindings) + " model binding(s))."
                        );
                    } else {
                        appendTerminalLine("Removed scene asset entry: " + selectedAsset.name + ".");
                    }
                    m_selectedAssetIndex = -1;
                }
            }
        }
    }


}
