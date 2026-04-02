#include "Engine/AssetManager.hpp"

#include "Core/FileSystem.hpp"
#include "Core/Log.hpp"
#include "Engine/AssetLoader.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Valkron {

    std::filesystem::path AssetManager::s_cachePath = AssetManager::getDefaultCachePath();

    namespace {

        enum class CachedAssetType {
            Texture2D,
            Texture3D,
            Model,
            ShaderVertex,
            ShaderFragment,
            ComputeShader,
            Unknown
        };

        struct CachedAssetEntry {
            CachedAssetType type = CachedAssetType::Unknown;
            std::string name;
            std::string path;
        };

        std::string toLowercase(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return value;
        }

        bool hasSuffix(const std::string& value, const std::string& suffix) {
            if (suffix.size() > value.size()) {
                return false;
            }
            return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
        }

        std::string stripSuffix(const std::string& value, const std::string& suffix) {
            if (!hasSuffix(value, suffix)) {
                return value;
            }
            return value.substr(0, value.size() - suffix.size());
        }

        CachedAssetType inferAssetType(const SceneAsset& asset) {
            const std::string extension = toLowercase(std::filesystem::path(asset.path).extension().string());
            const std::string lowerName = toLowercase(asset.name);
            const std::string lowerPath = toLowercase(asset.path);

            if (extension == ".vert" || extension == ".vs") {
                return CachedAssetType::ShaderVertex;
            }
            if (extension == ".frag" || extension == ".fs") {
                return CachedAssetType::ShaderFragment;
            }
            if (extension == ".comp") {
                return CachedAssetType::ComputeShader;
            }
            if (extension == ".obj" || extension == ".fbx" || extension == ".gltf" || extension == ".glb" || extension == ".dae" || extension == ".3ds") {
                return CachedAssetType::Model;
            }
            if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" || extension == ".tga" || extension == ".ppm") {
                const bool looks3D = lowerName.find("3d") != std::string::npos || lowerPath.find("volume") != std::string::npos;
                return looks3D ? CachedAssetType::Texture3D : CachedAssetType::Texture2D;
            }
            return CachedAssetType::Unknown;
        }

        std::string assetTypeToToken(CachedAssetType type) {
            switch (type) {
                case CachedAssetType::Texture2D:
                    return "texture2d";
                case CachedAssetType::Texture3D:
                    return "texture3d";
                case CachedAssetType::Model:
                    return "model";
                case CachedAssetType::ShaderVertex:
                    return "shader_vertex";
                case CachedAssetType::ShaderFragment:
                    return "shader_fragment";
                case CachedAssetType::ComputeShader:
                    return "compute";
                default:
                    return "unknown";
            }
        }

        CachedAssetType tokenToAssetType(const std::string& token) {
            if (token == "texture2d") {
                return CachedAssetType::Texture2D;
            }
            if (token == "texture3d") {
                return CachedAssetType::Texture3D;
            }
            if (token == "model") {
                return CachedAssetType::Model;
            }
            if (token == "shader_vertex") {
                return CachedAssetType::ShaderVertex;
            }
            if (token == "shader_fragment") {
                return CachedAssetType::ShaderFragment;
            }
            if (token == "compute") {
                return CachedAssetType::ComputeShader;
            }
            return CachedAssetType::Unknown;
        }

        std::vector<CachedAssetEntry> gatherCachedEntries(const Scene& scene) {
            std::vector<CachedAssetEntry> entries;
            entries.reserve(scene.getAssets().size());

            for (const SceneAsset& asset : scene.getAssets()) {
                const CachedAssetType type = inferAssetType(asset);
                if (type == CachedAssetType::Unknown) {
                    continue;
                }

                entries.push_back(CachedAssetEntry{type, asset.name, asset.path});
            }

            return entries;
        }

    }  // namespace

    std::filesystem::path AssetManager::getDefaultCachePath() {
        return FileSystem::resolveAssetPath("cache/runtime_assets.vkassetcache");
    }

    void AssetManager::setCachePath(const std::filesystem::path& cachePath) {
        s_cachePath = cachePath;
    }

    const std::filesystem::path& AssetManager::getCachePath() {
        return s_cachePath;
    }

    bool AssetManager::saveSceneAssetCache(const Scene& scene) {
        if (s_cachePath.empty()) {
            LOG_ERROR("AssetManager cache path is empty; cannot save cache.");
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(s_cachePath.parent_path(), ec);

        std::ofstream output(s_cachePath, std::ios::trunc);
        if (!output.is_open()) {
            LOG_ERROR("Failed to open asset cache for writing: " + s_cachePath.string());
            return false;
        }

        output << "VKASSETCACHE 1\n";

        const std::vector<CachedAssetEntry> entries = gatherCachedEntries(scene);
        for (const CachedAssetEntry& entry : entries) {
            output << "ASSET "
                   << assetTypeToToken(entry.type) << " "
                   << std::quoted(entry.name) << " "
                   << std::quoted(entry.path) << "\n";
        }

        for (const SceneEntity& entity : scene.getEntityData()) {
            if (entity.modelAssetName.empty()) {
                continue;
            }

            const std::string shaderName = AssetLoader::getModelShader(entity.modelAssetName);
            output << "ENTITY "
                   << std::quoted(entity.name) << " "
                   << std::quoted(entity.modelAssetName) << " "
                   << std::quoted(shaderName) << "\n";
        }

        LOG_INFO("Saved asset cache: " + s_cachePath.string());
        return true;
    }

    bool AssetManager::loadSceneAssetCache(Scene& scene) {
        if (s_cachePath.empty()) {
            LOG_ERROR("AssetManager cache path is empty; cannot load cache.");
            return false;
        }

        std::ifstream input(s_cachePath);
        if (!input.is_open()) {
            LOG_WARN("Asset cache not found: " + s_cachePath.string());
            return false;
        }

        std::string header;
        std::getline(input, header);
        if (header.rfind("VKASSETCACHE", 0) != 0) {
            LOG_ERROR("Invalid asset cache header in " + s_cachePath.string());
            return false;
        }

        std::vector<CachedAssetEntry> entries;
        struct EntityBinding {
            std::string entityName;
            std::string modelName;
            std::string shaderName;
        };
        std::vector<EntityBinding> entityBindings;

        std::string line;
        while (std::getline(input, line)) {
            if (line.empty()) {
                continue;
            }

            std::istringstream stream(line);
            std::string token;
            stream >> token;
            if (token == "ASSET") {
                std::string typeToken;
                std::string name;
                std::string path;
                stream >> typeToken >> std::quoted(name) >> std::quoted(path);
                if (typeToken.empty() || name.empty() || path.empty()) {
                    continue;
                }

                entries.push_back(CachedAssetEntry{tokenToAssetType(typeToken), name, path});
                continue;
            }

            if (token == "ENTITY") {
                EntityBinding binding;
                stream >> std::quoted(binding.entityName) >> std::quoted(binding.modelName) >> std::quoted(binding.shaderName);
                if (!binding.entityName.empty() && !binding.modelName.empty()) {
                    entityBindings.push_back(std::move(binding));
                }
            }
        }

        int loadedCount = 0;

        std::unordered_map<std::string, std::string> vertexShaders;
        std::unordered_map<std::string, std::string> fragmentShaders;
        for (const CachedAssetEntry& entry : entries) {
            if (entry.type == CachedAssetType::ShaderVertex) {
                vertexShaders[stripSuffix(entry.name, "_vert")] = entry.path;
            } else if (entry.type == CachedAssetType::ShaderFragment) {
                fragmentShaders[stripSuffix(entry.name, "_frag")] = entry.path;
            }
        }

        for (const auto& [shaderBaseName, vertexPath] : vertexShaders) {
            const auto fragmentIt = fragmentShaders.find(shaderBaseName);
            if (fragmentIt == fragmentShaders.end()) {
                continue;
            }

            if (AssetLoader::loadShader(shaderBaseName, vertexPath, fragmentIt->second)) {
                scene.addAsset(shaderBaseName + "_vert", vertexPath);
                scene.addAsset(shaderBaseName + "_frag", fragmentIt->second);
                ++loadedCount;
            }
        }

        for (const CachedAssetEntry& entry : entries) {
            switch (entry.type) {
                case CachedAssetType::Texture2D:
                    if (AssetLoader::loadTexture2D(entry.name, entry.path)) {
                        scene.addAsset(entry.name, entry.path);
                        ++loadedCount;
                    }
                    break;
                case CachedAssetType::Texture3D:
                    if (AssetLoader::loadTexture3D(entry.name, entry.path, 8)) {
                        scene.addAsset(entry.name, entry.path);
                        ++loadedCount;
                    }
                    break;
                case CachedAssetType::Model:
                    if (AssetLoader::loadModel(entry.name, entry.path)) {
                        scene.addAsset(entry.name, entry.path);
                        ++loadedCount;
                    }
                    break;
                case CachedAssetType::ComputeShader:
                    if (AssetLoader::loadComputeShader(entry.name, entry.path)) {
                        scene.addAsset(entry.name, entry.path);
                        ++loadedCount;
                    }
                    break;
                case CachedAssetType::ShaderVertex:
                case CachedAssetType::ShaderFragment:
                case CachedAssetType::Unknown:
                    break;
            }
        }

        const std::vector<SceneEntity>& entities = scene.getEntityData();
        for (const EntityBinding& binding : entityBindings) {
            const std::optional<std::size_t> entityIndex = scene.findEntityIndex(binding.entityName);
            if (!entityIndex.has_value()) {
                continue;
            }

            SceneEntity* entity = scene.getEntityByIndex(entityIndex.value());
            if (entity == nullptr) {
                continue;
            }

            entity->modelAssetName = binding.modelName;
            if (!binding.shaderName.empty()) {
                AssetLoader::setModelShader(binding.modelName, binding.shaderName);
            }

            (void)entities;
        }

        LOG_INFO("Loaded asset cache: " + s_cachePath.string() + " (entries loaded: " + std::to_string(loadedCount) + ")");
        return true;
    }

}
