#include "Application/UI/Panels/UILayerPanelsInternal.hpp"

#include <filesystem>
#include <unordered_map>

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

        std::vector<std::size_t> cameraEntityIndices;
        cameraEntityIndices.reserve(entities.size());
        for (std::size_t i = 0; i < entities.size(); ++i) {
            if (entities[i].type == SceneEntityType::Camera) {
                cameraEntityIndices.push_back(i);
            }
        }

        std::string primaryPlayCameraName = m_activeScene.getGameStateValue("PrimaryCameraEntity").value_or("");
        const bool hasStoredPrimaryCamera = std::any_of(cameraEntityIndices.begin(), cameraEntityIndices.end(), [&](std::size_t entityIndex) {
            return entities[entityIndex].name == primaryPlayCameraName;
        });

        if (!cameraEntityIndices.empty() && !hasStoredPrimaryCamera) {
            primaryPlayCameraName = entities[cameraEntityIndices.front()].name;
            m_activeScene.setGameStateValue("PrimaryCameraEntity", primaryPlayCameraName);
        }

        const char* playCameraPreview = primaryPlayCameraName.empty() ? "None" : primaryPlayCameraName.c_str();
        if (ImGui::BeginCombo("Main Play Camera", playCameraPreview)) {
            const bool noneSelected = primaryPlayCameraName.empty();
            if (ImGui::Selectable("None", noneSelected)) {
                m_activeScene.setGameStateValue("PrimaryCameraEntity", "");
                appendTerminalLine("Main play camera cleared.");
            }

            for (std::size_t cameraEntityIndex : cameraEntityIndices) {
                const bool selectedCamera = entities[cameraEntityIndex].name == primaryPlayCameraName;
                if (ImGui::Selectable(entities[cameraEntityIndex].name.c_str(), selectedCamera)) {
                    m_activeScene.setGameStateValue("PrimaryCameraEntity", entities[cameraEntityIndex].name);
                    appendTerminalLine(entities[cameraEntityIndex].name + " selected as main play camera.");
                }

                if (selectedCamera) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

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

        auto resolveSelectedEntity = [this]() -> SceneEntity* {
            const auto& refreshedEntities = m_activeScene.getEntityData();
            if (m_selectedEntityIndex < 0 || m_selectedEntityIndex >= static_cast<int>(refreshedEntities.size())) {
                clearEntitySelection();
                return nullptr;
            }

            return m_activeScene.getEntityByIndex(static_cast<std::size_t>(m_selectedEntityIndex));
        };

        auto drawTextureAssetBindingCombo = [](const char* label, std::string& linkedTextureName, const std::vector<std::string>& textureNames) {
            const char* preview = linkedTextureName.empty() ? "None" : linkedTextureName.c_str();
            if (ImGui::BeginCombo(label, preview)) {
                const bool noTextureSelected = linkedTextureName.empty();
                if (ImGui::Selectable("None", noTextureSelected)) {
                    linkedTextureName.clear();
                }

                for (const std::string& textureName : textureNames) {
                    const bool selectedTexture = linkedTextureName == textureName;
                    if (ImGui::Selectable(textureName.c_str(), selectedTexture)) {
                        linkedTextureName = textureName;
                    }

                    if (selectedTexture) {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
        };

        auto autoAssignPbrTextureLinks = [this](SceneEntity& entity) -> int {
            if (entity.modelAssetName.empty()) {
                return 0;
            }

            std::shared_ptr<Model> model = AssetLoader::getModel(entity.modelAssetName);
            if (model == nullptr || !model->isLoaded()) {
                const auto& sceneAssets = m_activeScene.getAssets();
                const auto modelAssetIt = std::find_if(sceneAssets.begin(), sceneAssets.end(), [&entity](const SceneAsset& asset) {
                    return asset.name == entity.modelAssetName && isModelSceneAsset(asset);
                });

                if (modelAssetIt != sceneAssets.end()) {
                    AssetLoader::loadModel(entity.modelAssetName, modelAssetIt->path);
                    model = AssetLoader::getModel(entity.modelAssetName);
                }
            }

            if (model == nullptr || !model->isLoaded()) {
                return 0;
            }

            const std::vector<std::string>& referencedTexturePaths = model->getReferencedTexturePaths();
            if (referencedTexturePaths.empty()) {
                return 0;
            }

            std::unordered_map<std::string, std::string> pathToAssetName;
            std::unordered_map<std::string, std::string> filenameToAssetName;
            for (const SceneAsset& asset : m_activeScene.getAssets()) {
                if (!isAssetPathAllowedForMode(asset.path, RuntimeImportMode::Texture2D)) {
                    continue;
                }

                const std::filesystem::path assetPath = std::filesystem::path(asset.path).lexically_normal();
                const std::string normalizedPath = toLowercase(assetPath.generic_string());
                const std::string filename = toLowercase(assetPath.filename().string());

                if (!normalizedPath.empty() && pathToAssetName.find(normalizedPath) == pathToAssetName.end()) {
                    pathToAssetName.emplace(normalizedPath, asset.name);
                }

                if (!filename.empty() && filenameToAssetName.find(filename) == filenameToAssetName.end()) {
                    filenameToAssetName.emplace(filename, asset.name);
                }
            }

            auto resolveAssetName = [&](const std::string& texturePath) -> std::string {
                const std::filesystem::path normalizedTexturePath = std::filesystem::path(texturePath).lexically_normal();
                const std::string normalizedPath = toLowercase(normalizedTexturePath.generic_string());
                const std::string filename = toLowercase(normalizedTexturePath.filename().string());

                const auto directPathMatch = pathToAssetName.find(normalizedPath);
                if (directPathMatch != pathToAssetName.end()) {
                    return directPathMatch->second;
                }

                const auto filenameMatch = filenameToAssetName.find(filename);
                if (filenameMatch != filenameToAssetName.end()) {
                    return filenameMatch->second;
                }

                return {};
            };

            auto hasAnyToken = [](const std::string& source, std::initializer_list<const char*> tokens) {
                for (const char* token : tokens) {
                    if (source.find(token) != std::string::npos) {
                        return true;
                    }
                }

                return false;
            };

            int assignedCount = 0;
            auto assignIfEmpty = [&assignedCount](std::string& destination, const std::string& textureAssetName) {
                if (!destination.empty() || textureAssetName.empty()) {
                    return false;
                }

                destination = textureAssetName;
                ++assignedCount;
                return true;
            };

            for (const std::string& sourceTexturePath : referencedTexturePaths) {
                const std::string textureAssetName = resolveAssetName(sourceTexturePath);
                if (textureAssetName.empty()) {
                    continue;
                }

                const std::string textureToken = toLowercase(std::filesystem::path(sourceTexturePath).stem().string());
                if (hasAnyToken(textureToken, {"normal", "norm", "nrm", "bump"})) {
                    assignIfEmpty(entity.shaderComponent.pbrMaterial.normalTextureAsset, textureAssetName);
                    continue;
                }

                if (hasAnyToken(textureToken, {"rough", "roughness", "rgh", "gloss"})) {
                    assignIfEmpty(entity.shaderComponent.pbrMaterial.roughnessTextureAsset, textureAssetName);
                    continue;
                }

                if (hasAnyToken(textureToken, {"metal", "metallic", "metalness"})) {
                    assignIfEmpty(entity.shaderComponent.pbrMaterial.metallicTextureAsset, textureAssetName);
                    continue;
                }

                if (hasAnyToken(textureToken, {"ao", "occlusion", "ambientocclusion", "ambient_occlusion"})) {
                    assignIfEmpty(entity.shaderComponent.pbrMaterial.aoTextureAsset, textureAssetName);
                    continue;
                }

                if (hasAnyToken(textureToken, {"alpha", "opacity", "transparency", "mask", "cutout"})) {
                    assignIfEmpty(entity.shaderComponent.pbrMaterial.alphaTextureAsset, textureAssetName);
                    continue;
                }

                if (hasAnyToken(textureToken, {"albedo", "basecolor", "base_color", "diffuse", "color", "col"})) {
                    assignIfEmpty(entity.shaderComponent.pbrMaterial.diffuseTextureAsset, textureAssetName);
                    if (entity.shaderComponent.pbrMaterial.albedoTextureAsset.empty()) {
                        entity.shaderComponent.pbrMaterial.albedoTextureAsset = textureAssetName;
                    }
                }
            }

            return assignedCount;
        };

        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::BeginTabBar("##InspectorEntityTabs")) {
            if (ImGui::BeginTabItem("Entity")) {
                selectedEntity = resolveSelectedEntity();
                if (selectedEntity != nullptr) {
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

                                const std::string primaryCameraName = m_activeScene.getGameStateValue("PrimaryCameraEntity").value_or("");
                                if (!primaryCameraName.empty() && primaryCameraName == oldName) {
                                    m_activeScene.setGameStateValue("PrimaryCameraEntity", appliedName);
                                }

                                std::fill(m_selectedEntityNameBuffer.begin(), m_selectedEntityNameBuffer.end(), '\0');
                                std::snprintf(m_selectedEntityNameBuffer.data(), m_selectedEntityNameBuffer.size(), "%s", appliedName.c_str());
                            }
                        }
                    }

                    selectedEntity = resolveSelectedEntity();
                    if (selectedEntity != nullptr) {
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
                                    const bool wasPrimaryPlayCamera = m_activeScene.getGameStateValue("PrimaryCameraEntity").value_or("") == selectedEntity->name;
                                    selectedEntity->type = typeOption;
                                    if (selectedEntity->type != SceneEntityType::Generic) {
                                        selectedEntity->modelAssetName.clear();
                                        selectedEntity->shaderComponent = SceneShaderComponent{};
                                    }

                                    if (wasPrimaryPlayCamera && selectedEntity->type != SceneEntityType::Camera) {
                                        std::string fallbackCameraName;
                                        const auto& allEntities = m_activeScene.getEntityData();
                                        for (const SceneEntity& entityOption : allEntities) {
                                            if (entityOption.type == SceneEntityType::Camera) {
                                                fallbackCameraName = entityOption.name;
                                                break;
                                            }
                                        }

                                        m_activeScene.setGameStateValue("PrimaryCameraEntity", fallbackCameraName);
                                    }

                                    appendTerminalLine("Set entity type for " + selectedEntity->name + " to " + std::string(typeLabel) + ".");
                                }

                                if (selectedType) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }

                            ImGui::EndCombo();
                        }

                        if (selectedEntity->type == SceneEntityType::Camera) {
                            const bool isMainPlayCamera = m_activeScene.getGameStateValue("PrimaryCameraEntity").value_or("") == selectedEntity->name;
                            if (!isMainPlayCamera) {
                                if (ImGui::Button("Set As Main Camera")) {
                                    m_activeScene.setGameStateValue("PrimaryCameraEntity", selectedEntity->name);
                                    appendTerminalLine(selectedEntity->name + " selected as main play camera.");
                                }
                            } else {
                                ImGui::TextDisabled("This camera is assigned as the main play camera.");
                            }
                        } else if (selectedEntity->type == SceneEntityType::Light) {
                            ImGui::Separator();
                            ImGui::Text("Light Properties");

                            const auto lightTypeName = [](SceneLightType type) {
                                return type == SceneLightType::Point ? "Point" : "Directional";
                            };

                            if (ImGui::BeginCombo("Light Type", lightTypeName(selectedEntity->lightComponent.type))) {
                                const std::array<SceneLightType, 2> lightTypes = {
                                    SceneLightType::Directional,
                                    SceneLightType::Point
                                };

                                for (const SceneLightType lightType : lightTypes) {
                                    const bool selectedLightType = selectedEntity->lightComponent.type == lightType;
                                    const char* label = lightTypeName(lightType);
                                    if (ImGui::Selectable(label, selectedLightType)) {
                                        selectedEntity->lightComponent.type = lightType;
                                    }

                                    if (selectedLightType) {
                                        ImGui::SetItemDefaultFocus();
                                    }
                                }

                                ImGui::EndCombo();
                            }

                            ImGui::ColorEdit3(
                                "Light Color",
                                &selectedEntity->lightComponent.color.x,
                                ImGuiColorEditFlags_Float
                            );
                            ImGui::SliderFloat("Light Intensity", &selectedEntity->lightComponent.intensity, 0.01f, 12.0f, "%.3f");
                            ImGui::SliderFloat("Ambient Strength", &selectedEntity->lightComponent.ambientStrength, 0.0f, 1.0f, "%.3f");

                            if (selectedEntity->lightComponent.type == SceneLightType::Point) {
                                ImGui::SliderFloat("Light Range", &selectedEntity->lightComponent.range, 0.25f, 200.0f, "%.2f");
                            }

                            selectedEntity->lightComponent.color.x = std::max(0.0f, selectedEntity->lightComponent.color.x);
                            selectedEntity->lightComponent.color.y = std::max(0.0f, selectedEntity->lightComponent.color.y);
                            selectedEntity->lightComponent.color.z = std::max(0.0f, selectedEntity->lightComponent.color.z);
                            selectedEntity->lightComponent.intensity = std::max(0.01f, selectedEntity->lightComponent.intensity);
                            selectedEntity->lightComponent.ambientStrength = std::clamp(selectedEntity->lightComponent.ambientStrength, 0.0f, 1.0f);
                            selectedEntity->lightComponent.range = std::max(0.25f, selectedEntity->lightComponent.range);
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
                                selectedEntity->modelMeshIndices.clear();
                                selectedEntity->applyModelNodeTransforms = true;
                            }

                            const std::vector<std::string> modelNames = AssetLoader::getModelNames();
                            for (const std::string& modelName : modelNames) {
                                const bool currentlySelectedModel = selectedEntity->modelAssetName == modelName;
                                if (ImGui::Selectable(modelName.c_str(), currentlySelectedModel)) {
                                    selectedEntity->modelAssetName = modelName;
                                    selectedEntity->modelMeshIndices.clear();
                                    selectedEntity->applyModelNodeTransforms = true;
                                    ensureEntityUsesPbrComponent(*selectedEntity);
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

                        const auto& refreshedEntities = m_activeScene.getEntityData();
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
                    }
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Components")) {
                selectedEntity = resolveSelectedEntity();
                if (selectedEntity != nullptr) {
                    ImGui::Text("Component Stack");
                    ImGui::Separator();

                    const bool supportsShaderComponent =
                        selectedEntity->type == SceneEntityType::Generic &&
                        !selectedEntity->modelAssetName.empty();

                    if (!supportsShaderComponent) {
                        ImGui::TextDisabled("Shader component requires a Generic entity with a model binding.");
                    }

                    const auto addShaderComponent = [&]() {
                        selectedEntity->shaderComponent.enabled = true;
                        if (selectedEntity->shaderComponent.shaderName.empty()) {
                            if (AssetLoader::getShader("PBR") != nullptr) {
                                selectedEntity->shaderComponent.shaderName = "PBR";
                            } else {
                                const std::vector<std::string> shaderNames = AssetLoader::getShaderNames();
                                if (!shaderNames.empty()) {
                                    selectedEntity->shaderComponent.shaderName = shaderNames.front();
                                }
                            }
                        }

                        if (selectedEntity->shaderComponent.shaderName == "PBR") {
                            const int autoAssignedTextureLinks = autoAssignPbrTextureLinks(*selectedEntity);
                            if (autoAssignedTextureLinks > 0) {
                                appendTerminalLine(
                                    "Auto-linked " + std::to_string(autoAssignedTextureLinks) +
                                    " PBR texture map(s) for " + selectedEntity->name + "."
                                );
                            }
                        }

                        appendTerminalLine("Added shader component to entity " + selectedEntity->name + ".");
                    };

                    if (ImGui::Button("Add Component")) {
                        ImGui::OpenPopup("AddEntityComponentPopup");
                    }

                    if (ImGui::BeginPopup("AddEntityComponentPopup")) {
                        if (ImGui::BeginMenu("Shaders")) {
                            if (!supportsShaderComponent) {
                                ImGui::BeginDisabled();
                            }

                            if (!selectedEntity->shaderComponent.enabled) {
                                if (ImGui::MenuItem("Shader Material")) {
                                    addShaderComponent();
                                }
                            } else {
                                ImGui::TextDisabled("Shader Material (already added)");
                            }

                            if (!supportsShaderComponent) {
                                ImGui::EndDisabled();
                            }

                            ImGui::EndMenu();
                        }

                        if (ImGui::BeginMenu("Colliders")) {
                            ImGui::MenuItem("Box Collider", nullptr, false, false);
                            ImGui::MenuItem("Sphere Collider", nullptr, false, false);
                            ImGui::MenuItem("Capsule Collider", nullptr, false, false);
                            ImGui::EndMenu();
                        }

                        if (ImGui::BeginMenu("Scripts")) {
                            ImGui::MenuItem("Runtime Script", nullptr, false, false);
                            ImGui::MenuItem("Native Script", nullptr, false, false);
                            ImGui::EndMenu();
                        }

                        ImGui::EndPopup();
                    }

                    if (selectedEntity->shaderComponent.enabled) {
                        if (ImGui::Button("Remove Shader Component")) {
                            selectedEntity->shaderComponent.enabled = false;
                            appendTerminalLine("Removed shader component from entity " + selectedEntity->name + ".");
                        }

                        if (!supportsShaderComponent) {
                            ImGui::BeginDisabled();
                        }

                        const char* shaderPreview = selectedEntity->shaderComponent.shaderName.empty() ? "None" : selectedEntity->shaderComponent.shaderName.c_str();
                        if (ImGui::BeginCombo("Shader", shaderPreview)) {
                            if (ImGui::Selectable("None", selectedEntity->shaderComponent.shaderName.empty())) {
                                selectedEntity->shaderComponent.shaderName.clear();
                            }

                            const std::vector<std::string> shaderNames = AssetLoader::getShaderNames();
                            for (const std::string& shaderName : shaderNames) {
                                const bool selectedShader = selectedEntity->shaderComponent.shaderName == shaderName;
                                if (ImGui::Selectable(shaderName.c_str(), selectedShader)) {
                                    selectedEntity->shaderComponent.shaderName = shaderName;

                                    if (shaderName == "PBR") {
                                        const int autoAssignedTextureLinks = autoAssignPbrTextureLinks(*selectedEntity);
                                        if (autoAssignedTextureLinks > 0) {
                                            appendTerminalLine(
                                                "Auto-linked " + std::to_string(autoAssignedTextureLinks) +
                                                " PBR texture map(s) for " + selectedEntity->name + "."
                                            );
                                        }
                                    }

                                    appendTerminalLine(
                                        "Assigned shader " + shaderName + " to entity " + selectedEntity->name + "."
                                    );
                                }

                                if (selectedShader) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }

                            ImGui::EndCombo();
                        }

                        const std::vector<std::string> texture2DNames = AssetLoader::getTexture2DNames();
                        if (selectedEntity->shaderComponent.shaderName == "PBR") {
                            ImGui::Separator();
                            ImGui::Text("PBR Material Attributes");

                            ImGui::ColorEdit3(
                                "Albedo Color",
                                &selectedEntity->shaderComponent.pbrMaterial.albedoColor.x,
                                ImGuiColorEditFlags_Float
                            );
                            ImGui::SliderFloat("Metallic", &selectedEntity->shaderComponent.pbrMaterial.metallic, 0.0f, 1.0f, "%.3f");
                            ImGui::SliderFloat("Roughness", &selectedEntity->shaderComponent.pbrMaterial.roughness, 0.04f, 1.0f, "%.3f");
                            ImGui::SliderFloat("AO", &selectedEntity->shaderComponent.pbrMaterial.ambientOcclusion, 0.0f, 1.0f, "%.3f");

                            drawTextureAssetBindingCombo("Albedo Map", selectedEntity->shaderComponent.pbrMaterial.albedoTextureAsset, texture2DNames);
                            drawTextureAssetBindingCombo("Diffuse Map", selectedEntity->shaderComponent.pbrMaterial.diffuseTextureAsset, texture2DNames);
                            drawTextureAssetBindingCombo("Alpha Map", selectedEntity->shaderComponent.pbrMaterial.alphaTextureAsset, texture2DNames);
                            drawTextureAssetBindingCombo("Normal Map", selectedEntity->shaderComponent.pbrMaterial.normalTextureAsset, texture2DNames);
                            drawTextureAssetBindingCombo("Metallic Map", selectedEntity->shaderComponent.pbrMaterial.metallicTextureAsset, texture2DNames);
                            drawTextureAssetBindingCombo("Roughness Map", selectedEntity->shaderComponent.pbrMaterial.roughnessTextureAsset, texture2DNames);
                            drawTextureAssetBindingCombo("AO Map", selectedEntity->shaderComponent.pbrMaterial.aoTextureAsset, texture2DNames);
                        } else if (selectedEntity->shaderComponent.shaderName == "Blinn-Phong") {
                            ImGui::Separator();
                            ImGui::Text("Material Texture Overrides");
                            drawTextureAssetBindingCombo("Albedo Map", selectedEntity->shaderComponent.pbrMaterial.albedoTextureAsset, texture2DNames);
                            drawTextureAssetBindingCombo("Diffuse Map", selectedEntity->shaderComponent.pbrMaterial.diffuseTextureAsset, texture2DNames);
                            drawTextureAssetBindingCombo("Alpha Map", selectedEntity->shaderComponent.pbrMaterial.alphaTextureAsset, texture2DNames);
                        } else {
                            ImGui::Separator();
                            ImGui::TextDisabled("Attribute linking is currently implemented for PBR and Blinn-Phong shaders.");
                        }

                        if (!supportsShaderComponent) {
                            ImGui::EndDisabled();
                        }
                    } else {
                        ImGui::TextDisabled("No components attached to this entity.");
                    }
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Transform")) {
                selectedEntity = resolveSelectedEntity();
                if (selectedEntity != nullptr) {
                    ImGui::Text("Transform");
                    ImGui::Separator();

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
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }


}
