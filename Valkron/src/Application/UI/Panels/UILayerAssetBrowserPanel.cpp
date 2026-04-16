#include "Application/UI/Panels/UILayerPanelsInternal.hpp"

namespace Valkron {

    void UILayer::drawBottomPanel() {
        if (m_showAssetManagerPanel) {
            if (ImGui::Begin("Asset Manager", &m_showAssetManagerPanel)) {
                drawWindowPanelGradient();
                drawAssetsPanel();
            }
            ImGui::End();
        }

        if (m_showConsolePanel) {
            if (ImGui::Begin("Console", &m_showConsolePanel)) {
                drawWindowPanelGradient();
                drawTerminalPanel();
            }
            ImGui::End();
        }
    }

    void UILayer::drawAssetsPanel() {
        constexpr float kFixedAssetIconSize = 72.0f;
        constexpr float kFolderIconSize = 56.0f;
        constexpr float kToolbarIconSize = 22.0f;
        ImGui::TextUnformatted("Asset Viewer");

        const auto& assets = m_activeScene.getAssets();

        std::string currentFolder = normalizeFolderPath(m_assetBrowserFolderFilter);
        if (currentFolder == "All") {
            currentFolder.clear();
        }
        if (currentFolder != m_assetBrowserFolderFilter) {
            m_assetBrowserFolderFilter = currentFolder;
        }

        if (m_assetBrowserNavigationHistory.empty()) {
            m_assetBrowserNavigationHistory.push_back(currentFolder);
            m_assetBrowserNavigationIndex = 0;
        }

        if (m_assetBrowserNavigationIndex < 0 || m_assetBrowserNavigationIndex >= static_cast<int>(m_assetBrowserNavigationHistory.size())) {
            m_assetBrowserNavigationIndex = static_cast<int>(m_assetBrowserNavigationHistory.size()) - 1;
        }

        if (m_assetBrowserNavigationIndex >= 0 &&
            m_assetBrowserNavigationIndex < static_cast<int>(m_assetBrowserNavigationHistory.size()) &&
            m_assetBrowserNavigationHistory[static_cast<std::size_t>(m_assetBrowserNavigationIndex)] != currentFolder) {
            m_assetBrowserNavigationHistory.erase(
                m_assetBrowserNavigationHistory.begin() + static_cast<std::ptrdiff_t>(m_assetBrowserNavigationIndex + 1),
                m_assetBrowserNavigationHistory.end()
            );
            m_assetBrowserNavigationHistory.push_back(currentFolder);
            m_assetBrowserNavigationIndex = static_cast<int>(m_assetBrowserNavigationHistory.size()) - 1;
        }

        auto navigateToFolder = [&](const std::string& targetFolder, bool recordInHistory) {
            std::string normalizedFolder = normalizeFolderPath(targetFolder);
            if (normalizedFolder == "All") {
                normalizedFolder.clear();
            }

            if (normalizedFolder == currentFolder) {
                return;
            }

            currentFolder = normalizedFolder;
            m_assetBrowserFolderFilter = normalizedFolder;
            m_selectedAssetIndex = -1;

            if (!recordInHistory) {
                return;
            }

            m_assetBrowserNavigationHistory.erase(
                m_assetBrowserNavigationHistory.begin() + static_cast<std::ptrdiff_t>(m_assetBrowserNavigationIndex + 1),
                m_assetBrowserNavigationHistory.end()
            );
            m_assetBrowserNavigationHistory.push_back(normalizedFolder);
            m_assetBrowserNavigationIndex = static_cast<int>(m_assetBrowserNavigationHistory.size()) - 1;
        };

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

        std::optional<std::size_t> pendingRemoveAssetIndex;

        const bool canGoBack = m_assetBrowserNavigationIndex > 0;
        const bool canGoForward = m_assetBrowserNavigationIndex >= 0 && m_assetBrowserNavigationIndex < static_cast<int>(m_assetBrowserNavigationHistory.size()) - 1;
        std::string displayPath = currentFolder.empty() ? "/" : ("/" + currentFolder);

        bool openImportBrowser = false;
        if (ImGui::BeginTable("AssetBrowserToolbar", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings)) {
            ImGui::TableSetupColumn("##AssetBrowserToolbarMain", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##AssetBrowserToolbarImport", ImGuiTableColumnFlags_WidthFixed, kToolbarIconSize + 8.0f);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            if (!canGoBack) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("<")) {
                --m_assetBrowserNavigationIndex;
                const std::string& targetFolder = m_assetBrowserNavigationHistory[static_cast<std::size_t>(m_assetBrowserNavigationIndex)];
                navigateToFolder(targetFolder, false);
            }
            if (!canGoBack) {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();
            if (!canGoForward) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button(">")) {
                ++m_assetBrowserNavigationIndex;
                const std::string& targetFolder = m_assetBrowserNavigationHistory[static_cast<std::size_t>(m_assetBrowserNavigationIndex)];
                navigateToFolder(targetFolder, false);
            }
            if (!canGoForward) {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();
            ImGui::Text("Path: %s", displayPath.c_str());

            ImGui::TableSetColumnIndex(1);
            if (m_assetImportIconTexture != nullptr && m_assetImportIconTexture->getID() != 0) {
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(18, 24, 30, 210));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(34, 44, 56, 235));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(42, 56, 72, 245));

                openImportBrowser = ImGui::ImageButton(
                    "##AssetImportIconButton",
                    (ImTextureID)(uintptr_t)m_assetImportIconTexture->getID(),
                    ImVec2(kToolbarIconSize, kToolbarIconSize),
                    ImVec2(0.0f, 0.0f),
                    ImVec2(1.0f, 1.0f)
                );

                ImGui::PopStyleColor(3);
            } else {
                openImportBrowser = ImGui::Button("Import");
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Import assets (models, shaders, textures, compute)");
            }

            ImGui::EndTable();
        }

        if (openImportBrowser) {
            loadRuntimeAssetAuto();
        }

        if (m_selectedAssetIndex >= static_cast<int>(assets.size())) {
            m_selectedAssetIndex = -1;
        }

        struct BrowserEntry {
            bool isFolder = false;
            std::string folderPath;
            std::size_t assetIndex = 0;
        };

        std::vector<BrowserEntry> browserEntries;
        browserEntries.reserve(childFolders.size() + filteredAssetIndices.size());
        for (const std::string& childFolderPath : childFolders) {
            browserEntries.push_back(BrowserEntry{true, childFolderPath, 0});
        }
        for (std::size_t assetIndex : filteredAssetIndices) {
            browserEntries.push_back(BrowserEntry{false, std::string{}, assetIndex});
        }

        const bool hasSelectedAsset = m_selectedAssetIndex >= 0 && m_selectedAssetIndex < static_cast<int>(assets.size());
        const bool hasSelectedModelActions = hasSelectedAsset && isModelSceneAsset(assets[static_cast<std::size_t>(m_selectedAssetIndex)]);
        const float assetGridHeight = hasSelectedModelActions ? -86.0f : -4.0f;
        if (ImGui::BeginChild("AssetBrowserGrid", ImVec2(0.0f, assetGridHeight), true)) {
            if (browserEntries.empty()) {
                ImGui::TextDisabled("Nothing to show in this folder.");
            } else {
                const float cellWidth = std::max(kFixedAssetIconSize, kFolderIconSize) + 16.0f;
                const int columns = std::max(1, static_cast<int>(std::max(1.0f, ImGui::GetContentRegionAvail().x) / cellWidth));

                if (ImGui::BeginTable("AssetBrowserUnifiedGrid", columns, ImGuiTableFlags_SizingStretchSame)) {
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f, 3.0f));
                    for (const BrowserEntry& entry : browserEntries) {
                        ImGui::TableNextColumn();

                        if (entry.isFolder) {
                            ImGui::PushID(entry.folderPath.c_str());

                            bool openFolder = false;
                            if (m_assetDirectoryIconTexture != nullptr && m_assetDirectoryIconTexture->getID() != 0) {
                                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(18, 26, 34, 210));
                                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(30, 44, 58, 230));
                                ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(40, 56, 74, 245));
                                openFolder = ImGui::ImageButton(
                                    "##AssetFolderIcon",
                                    (ImTextureID)(uintptr_t)m_assetDirectoryIconTexture->getID(),
                                    ImVec2(kFolderIconSize, kFolderIconSize),
                                    ImVec2(0.0f, 0.0f),
                                    ImVec2(1.0f, 1.0f)
                                );
                                ImGui::PopStyleColor(3);
                            } else {
                                openFolder = ImGui::Button("DIR", ImVec2(kFolderIconSize, kFolderIconSize));
                            }

                            if (openFolder) {
                                navigateToFolder(entry.folderPath, true);
                            }

                            if (ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("Open folder: %s", entry.folderPath.c_str());
                            }

                            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kFolderIconSize + 8.0f);
                            ImGui::TextWrapped("%s", getFolderLeafName(entry.folderPath).c_str());
                            ImGui::PopTextWrapPos();

                            ImGui::PopID();
                        } else {
                            const std::size_t assetIndex = entry.assetIndex;
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

                            if (ImGui::BeginPopupContextItem("AssetEntryContextMenu")) {
                                if (ImGui::MenuItem("Remove From Scene Asset List")) {
                                    pendingRemoveAssetIndex = assetIndex;
                                }
                                ImGui::EndPopup();
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
                    }

                    ImGui::PopStyleVar();

                    ImGui::EndTable();
                }
            }
        }
        ImGui::EndChild();

        if (hasSelectedModelActions) {
            const SceneAsset& selectedAsset = assets[static_cast<std::size_t>(m_selectedAssetIndex)];
            ImGui::Spacing();
            ImGui::Separator();
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
                            entity->modelMeshIndices.clear();
                            entity->applyModelNodeTransforms = true;
                            entity->transform.position = m_sceneCameraPivot;
                            ensureEntityUsesPbrComponent(*entity);
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

        if (pendingRemoveAssetIndex.has_value()) {
            const auto& currentAssets = m_activeScene.getAssets();
            if (pendingRemoveAssetIndex.value() < currentAssets.size()) {
                const SceneAsset assetToRemove = currentAssets[pendingRemoveAssetIndex.value()];

                int clearedBindings = 0;
                int clearedShaderTextureLinks = 0;
                if (isModelSceneAsset(assetToRemove)) {
                    const auto& entities = m_activeScene.getEntityData();
                    for (std::size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex) {
                        if (SceneEntity* entity = m_activeScene.getEntityByIndex(entityIndex); entity != nullptr && entity->modelAssetName == assetToRemove.name) {
                            entity->modelAssetName.clear();
                            entity->modelMeshIndices.clear();
                            entity->applyModelNodeTransforms = true;
                            entity->shaderComponent.enabled = false;
                            ++clearedBindings;
                        }
                    }
                }

                {
                    const auto& entities = m_activeScene.getEntityData();
                    for (std::size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex) {
                        SceneEntity* entity = m_activeScene.getEntityByIndex(entityIndex);
                        if (entity == nullptr) {
                            continue;
                        }

                        auto clearTextureLink = [&](std::string& linkedTextureName) {
                            if (linkedTextureName == assetToRemove.name) {
                                linkedTextureName.clear();
                                ++clearedShaderTextureLinks;
                            }
                        };

                        clearTextureLink(entity->shaderComponent.pbrMaterial.diffuseTextureAsset);
                        clearTextureLink(entity->shaderComponent.pbrMaterial.albedoTextureAsset);
                        clearTextureLink(entity->shaderComponent.pbrMaterial.alphaTextureAsset);
                        clearTextureLink(entity->shaderComponent.pbrMaterial.normalTextureAsset);
                        clearTextureLink(entity->shaderComponent.pbrMaterial.metallicTextureAsset);
                        clearTextureLink(entity->shaderComponent.pbrMaterial.roughnessTextureAsset);
                        clearTextureLink(entity->shaderComponent.pbrMaterial.aoTextureAsset);
                    }
                }

                if (m_activeScene.removeAsset(assetToRemove.name)) {
                    if (clearedBindings > 0 || clearedShaderTextureLinks > 0) {
                        std::string bindingMessage;
                        if (clearedBindings > 0) {
                            bindingMessage += "cleared " + std::to_string(clearedBindings) + " model binding(s)";
                        }
                        if (clearedShaderTextureLinks > 0) {
                            if (!bindingMessage.empty()) {
                                bindingMessage += ", ";
                            }
                            bindingMessage += "cleared " + std::to_string(clearedShaderTextureLinks) + " shader texture link(s)";
                        }

                        appendTerminalLine(
                            "Removed scene asset entry: " + assetToRemove.name +
                            " (" + bindingMessage + ")."
                        );
                    } else {
                        appendTerminalLine("Removed scene asset entry: " + assetToRemove.name + ".");
                    }

                    const int removedIndex = static_cast<int>(pendingRemoveAssetIndex.value());
                    if (m_selectedAssetIndex == removedIndex) {
                        m_selectedAssetIndex = -1;
                    } else if (m_selectedAssetIndex > removedIndex) {
                        --m_selectedAssetIndex;
                    }
                }
            }
        }
    }


}
