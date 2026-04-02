#include "Application/UI/RuntimeAssetImportService.hpp"

#include "Core/FileSystem.hpp"
#include "Core/Log.hpp"
#include "Engine/AssetLoader.hpp"
#include "Renderer/Model.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Valkron {

    namespace {

        std::string toLowercase(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return value;
        }

        std::string deriveAssetBaseName(const std::string& absoluteOrRelativePath, const std::string& fallbackName) {
            const std::filesystem::path filePath(absoluteOrRelativePath);
            const std::string stem = filePath.stem().string();
            return stem.empty() ? fallbackName : stem;
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

        int registerImportedModelMaterialTextures(Scene& scene, const std::string& modelName, std::vector<std::string>& outMessages) {
            const std::shared_ptr<Model> model = AssetLoader::getModel(modelName);
            if (model == nullptr || !model->isLoaded()) {
                return 0;
            }

            std::vector<std::string> existingNames = collectAllRuntimeAssetNames();
            std::unordered_set<std::string> existingPathKeys;
            existingPathKeys.reserve(scene.getAssets().size());
            for (const SceneAsset& existingAsset : scene.getAssets()) {
                existingPathKeys.insert(normalizePathKey(existingAsset.path));
                existingNames.push_back(existingAsset.name);
            }

            int importedTextureCount = 0;
            for (const std::string& texturePath : model->getReferencedTexturePaths()) {
                if (texturePath.empty()) {
                    continue;
                }

                const std::filesystem::path resolvedPath = FileSystem::resolveExistingPath(texturePath);
                const std::string resolvedPathString = resolvedPath.string();
                if (resolvedPathString.empty()) {
                    continue;
                }

                const std::string pathKey = normalizePathKey(resolvedPathString);
                if (existingPathKeys.find(pathKey) != existingPathKeys.end()) {
                    continue;
                }

                const std::string textureName = makeUniqueAssetName(deriveAssetBaseName(resolvedPathString, modelName + "_Texture"), existingNames);
                if (!AssetLoader::loadTexture2D(textureName, resolvedPathString)) {
                    LOG_WARN("Failed to preload model material texture: " + resolvedPathString);
                    continue;
                }

                scene.addAsset(textureName, resolvedPathString);
                existingNames.push_back(textureName);
                existingPathKeys.insert(pathKey);
                ++importedTextureCount;
            }

            if (importedTextureCount > 0) {
                outMessages.push_back(
                    "Imported " + std::to_string(importedTextureCount) +
                    " material texture(s) for model " + modelName + "."
                );
            }

            return importedTextureCount;
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

        const std::array<const char*, 6> kTextureExtensions = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".ppm"};
        const std::array<const char*, 8> kShaderExtensions = {".vert", ".vs", ".frag", ".fs", ".comp", ".glsl", ".hlsl", ".wgsl"};
        const std::array<const char*, 6> kModelExtensions = {".obj", ".fbx", ".gltf", ".glb", ".dae", ".3ds"};

    }  // namespace

    bool RuntimeAssetImportService::isPathAllowedForMode(const std::filesystem::path& path, RuntimeImportMode mode) {
        if (!path.has_extension()) {
            return false;
        }

        const std::string extension = toLowercase(path.extension().string());
        const auto hasExtension = [&extension](const auto& values) {
            return std::find_if(values.begin(), values.end(), [&extension](const char* value) {
                return extension == value;
            }) != values.end();
        };

        switch (mode) {
            case RuntimeImportMode::Texture2D:
            case RuntimeImportMode::Texture3D:
                return hasExtension(kTextureExtensions);
            case RuntimeImportMode::Model:
                return hasExtension(kModelExtensions);
            case RuntimeImportMode::Shader:
                return extension == ".vert" || extension == ".vs" || extension == ".frag" || extension == ".fs" || extension == ".glsl";
            case RuntimeImportMode::Compute:
                return extension == ".comp" || extension == ".glsl";
            case RuntimeImportMode::Auto:
                return hasExtension(kTextureExtensions) || hasExtension(kModelExtensions) || hasExtension(kShaderExtensions);
        }

        return false;
    }

    RuntimeAssetImportResult RuntimeAssetImportService::importAsset(Scene& scene, const std::string& assetPath, RuntimeImportMode mode, int texture3DDepth) {
        RuntimeAssetImportResult result;

        if (assetPath.empty()) {
            result.messages.push_back("Import cancelled: no file selected.");
            return result;
        }

        const std::filesystem::path selectedPath(assetPath);
        const std::string extension = toLowercase(selectedPath.extension().string());
        if (extension.empty()) {
            result.messages.push_back("Import cancelled: selected file has no extension.");
            return result;
        }

        const std::vector<std::string> existingNames = collectAllRuntimeAssetNames();

        if (mode == RuntimeImportMode::Texture2D || mode == RuntimeImportMode::Texture3D) {
            if (!isTextureImageExtension(extension)) {
                result.messages.push_back("Unsupported texture format: " + assetPath);
                return result;
            }

            const bool importAsVolume = (mode == RuntimeImportMode::Texture3D);
            const std::string fallbackName = importAsVolume ? "Texture3D" : "Texture2D";
            const std::string textureName = makeUniqueAssetName(deriveAssetBaseName(assetPath, fallbackName), existingNames);
            const bool loaded = importAsVolume
                ? AssetLoader::loadTexture3D(textureName, assetPath, std::max(1, texture3DDepth))
                : AssetLoader::loadTexture2D(textureName, assetPath);

            if (!loaded) {
                result.messages.push_back("Failed to import texture asset: " + assetPath);
                return result;
            }

            scene.addAsset(textureName, assetPath);
            result.messages.push_back("Imported texture: " + textureName + (importAsVolume ? " (3D)" : " (2D)"));
            result.success = true;
            return result;
        }

        if (mode == RuntimeImportMode::Model) {
            if (!isModelAssetExtension(extension)) {
                result.messages.push_back("Unsupported model format: " + assetPath);
                return result;
            }

            const std::string modelName = makeUniqueAssetName(deriveAssetBaseName(assetPath, "Model"), existingNames);
            if (!AssetLoader::loadModel(modelName, assetPath)) {
                result.messages.push_back("Failed to import model asset: " + assetPath);
                return result;
            }

            scene.addAsset(modelName, assetPath);
            (void)registerImportedModelMaterialTextures(scene, modelName, result.messages);
            result.messages.push_back("Imported model asset: " + modelName + ". Drag it into Scene View to create an entity.");
            result.success = true;
            return result;
        }

        if (mode == RuntimeImportMode::Compute) {
            if (!(isComputeShaderExtension(extension) || extension == ".glsl")) {
                result.messages.push_back("Unsupported compute shader format: " + assetPath);
                return result;
            }

            const std::string computeName = makeUniqueAssetName(deriveAssetBaseName(assetPath, "Compute"), existingNames);
            if (!AssetLoader::loadComputeShader(computeName, assetPath)) {
                result.messages.push_back("Failed to import compute shader: " + assetPath);
                return result;
            }

            scene.addAsset(computeName, assetPath);
            result.messages.push_back("Imported compute shader: " + computeName);
            result.success = true;
            return result;
        }

        if (mode == RuntimeImportMode::Shader) {
            const std::optional<std::pair<std::string, std::string>> shaderPair = resolveShaderPair(selectedPath);
            if (!shaderPair.has_value()) {
                result.messages.push_back("Unable to resolve shader pair for import.");
                return result;
            }

            std::string baseName = deriveAssetBaseName(shaderPair->first, "Shader");
            if (baseName.ends_with("_vert")) {
                baseName = baseName.substr(0, baseName.size() - 5);
            }

            const std::string shaderName = makeUniqueAssetName(baseName, existingNames);
            if (!AssetLoader::loadShader(shaderName, shaderPair->first, shaderPair->second)) {
                result.messages.push_back("Failed to import shader pair: " + shaderPair->first + " + " + shaderPair->second);
                return result;
            }

            scene.addAsset(shaderName + "_vert", shaderPair->first);
            scene.addAsset(shaderName + "_frag", shaderPair->second);
            result.messages.push_back("Imported shader pair: " + shaderName);
            result.success = true;
            return result;
        }

        if (mode == RuntimeImportMode::Auto) {
            if (isTextureImageExtension(extension)) {
                const std::string lowerPath = toLowercase(assetPath);
                const RuntimeImportMode textureMode = (lowerPath.find("_3d") != std::string::npos || lowerPath.find("volume") != std::string::npos)
                    ? RuntimeImportMode::Texture3D
                    : RuntimeImportMode::Texture2D;
                return importAsset(scene, assetPath, textureMode, texture3DDepth);
            }

            if (isModelAssetExtension(extension)) {
                return importAsset(scene, assetPath, RuntimeImportMode::Model, texture3DDepth);
            }

            const std::string fileNameLower = toLowercase(selectedPath.filename().string());
            if (isComputeShaderExtension(extension) || (extension == ".glsl" && fileNameLower.find("comp") != std::string::npos)) {
                return importAsset(scene, assetPath, RuntimeImportMode::Compute, texture3DDepth);
            }

            if (isVertexShaderExtension(extension) || isFragmentShaderExtension(extension) || extension == ".glsl") {
                return importAsset(scene, assetPath, RuntimeImportMode::Shader, texture3DDepth);
            }

            result.messages.push_back("Unsupported asset type for import: " + assetPath);
            return result;
        }

        result.messages.push_back("Unsupported runtime import mode.");
        return result;
    }

}
