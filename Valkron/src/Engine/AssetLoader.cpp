#include "Engine/AssetLoader.hpp"

#include "Core/Log.hpp"
#include "Renderer/ComputeShader.hpp"
#include "Renderer/Model.hpp"
#include "Renderer/Shader.hpp"
#include "Renderer/Texture.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Valkron {

    static bool s_assetLoaderInitialized = false;
    static std::unordered_map<std::string, std::shared_ptr<Texture>> s_textures2D;
    static std::unordered_map<std::string, std::shared_ptr<Texture>> s_textures3D;
    static std::unordered_map<std::string, std::shared_ptr<Shader>> s_shaders;
    static std::unordered_map<std::string, std::shared_ptr<ComputeShader>> s_computeShaders;
    static std::unordered_map<std::string, std::shared_ptr<Model>> s_models;
    static std::unordered_map<std::string, std::string> s_modelShaderBindings;
    static std::string s_activeModelName;

    std::vector<std::string> mapKeys(const std::unordered_map<std::string, std::shared_ptr<Texture>>& map) {
        std::vector<std::string> keys;
        keys.reserve(map.size());
        for (const auto& [key, _] : map) {
            keys.push_back(key);
        }
        std::sort(keys.begin(), keys.end());
        return keys;
    }

    std::vector<std::string> mapKeys(const std::unordered_map<std::string, std::shared_ptr<Shader>>& map) {
        std::vector<std::string> keys;
        keys.reserve(map.size());
        for (const auto& [key, _] : map) {
            keys.push_back(key);
        }
        std::sort(keys.begin(), keys.end());
        return keys;
    }

    std::vector<std::string> mapKeys(const std::unordered_map<std::string, std::shared_ptr<ComputeShader>>& map) {
        std::vector<std::string> keys;
        keys.reserve(map.size());
        for (const auto& [key, _] : map) {
            keys.push_back(key);
        }
        std::sort(keys.begin(), keys.end());
        return keys;
    }

    std::vector<std::string> mapKeys(const std::unordered_map<std::string, std::shared_ptr<Model>>& map) {
        std::vector<std::string> keys;
        keys.reserve(map.size());
        for (const auto& [key, _] : map) {
            keys.push_back(key);
        }
        std::sort(keys.begin(), keys.end());
        return keys;
    }

    void AssetLoader::initialize() {
        if (s_assetLoaderInitialized) {
            return;
        }

        loadDefaultAssets();
        s_assetLoaderInitialized = true;
        LOG_INFO("AssetLoader initialized.");
    }

    bool AssetLoader::loadTexture2D(const std::string& name, const std::string& texturePath) {
        if (name.empty() || texturePath.empty()) {
            return false;
        }

        auto texture = std::make_shared<Texture>();
        if (!texture->loadTexture(texturePath, false)) {
            LOG_ERROR("Failed to load 2D texture asset '" + name + "' from " + texturePath);
            return false;
        }

        s_textures2D[name] = std::move(texture);
        return true;
    }

    bool AssetLoader::loadTexture3D(const std::string& name, const std::string& texturePath, int depth) {
        if (name.empty() || texturePath.empty()) {
            return false;
        }

        auto texture = std::make_shared<Texture>();
        if (!texture->loadTexture3D(texturePath, depth)) {
            LOG_ERROR("Failed to load 3D texture asset '" + name + "' from " + texturePath);
            return false;
        }

        s_textures3D[name] = std::move(texture);
        return true;
    }

    bool AssetLoader::loadShader(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath) {
        if (name.empty() || vertexPath.empty() || fragmentPath.empty()) {
            return false;
        }

        auto shader = std::make_shared<Shader>(vertexPath, fragmentPath);
        s_shaders[name] = std::move(shader);
        return true;
    }

    bool AssetLoader::loadComputeShader(const std::string& name, const std::string& computePath) {
        if (name.empty() || computePath.empty()) {
            return false;
        }

        auto shader = std::make_shared<ComputeShader>(computePath);
        s_computeShaders[name] = std::move(shader);
        return true;
    }

    bool AssetLoader::loadModel(const std::string& name, const std::string& modelPath) {
        if (name.empty() || modelPath.empty()) {
            return false;
        }

        auto model = std::make_shared<Model>();
        if (!model->loadFromFile(modelPath)) {
            LOG_ERROR("Failed to load model asset '" + name + "' from " + modelPath);
            return false;
        }

        s_models[name] = std::move(model);

        if (s_activeModelName.empty()) {
            s_activeModelName = name;
        }

        if (s_modelShaderBindings.find(name) == s_modelShaderBindings.end()) {
            auto defaultShader = s_shaders.find("Blinn-Phong");
            if (defaultShader != s_shaders.end()) {
                s_modelShaderBindings[name] = "Blinn-Phong";
            }
        }

        return true;
    }

    std::shared_ptr<Texture> AssetLoader::getTexture2D(const std::string& name) {
        auto it = s_textures2D.find(name);
        if (it == s_textures2D.end()) {
            return nullptr;
        }

        return it->second;
    }

    std::shared_ptr<Texture> AssetLoader::getTexture3D(const std::string& name) {
        auto it = s_textures3D.find(name);
        if (it == s_textures3D.end()) {
            return nullptr;
        }

        return it->second;
    }

    std::shared_ptr<Shader> AssetLoader::getShader(const std::string& name) {
        auto it = s_shaders.find(name);
        if (it == s_shaders.end()) {
            return nullptr;
        }

        return it->second;
    }

    std::shared_ptr<ComputeShader> AssetLoader::getComputeShader(const std::string& name) {
        auto it = s_computeShaders.find(name);
        if (it == s_computeShaders.end()) {
            return nullptr;
        }

        return it->second;
    }

    std::shared_ptr<Model> AssetLoader::getModel(const std::string& name) {
        auto it = s_models.find(name);
        if (it == s_models.end()) {
            return nullptr;
        }

        return it->second;
    }

    std::vector<std::string> AssetLoader::getTexture2DNames() {
        return mapKeys(s_textures2D);
    }

    std::vector<std::string> AssetLoader::getTexture3DNames() {
        return mapKeys(s_textures3D);
    }

    std::vector<std::string> AssetLoader::getShaderNames() {
        return mapKeys(s_shaders);
    }

    std::vector<std::string> AssetLoader::getComputeShaderNames() {
        return mapKeys(s_computeShaders);
    }

    std::vector<std::string> AssetLoader::getModelNames() {
        return mapKeys(s_models);
    }

    bool AssetLoader::setModelShader(const std::string& modelName, const std::string& shaderName) {
        if (s_models.find(modelName) == s_models.end()) {
            return false;
        }

        if (s_shaders.find(shaderName) == s_shaders.end()) {
            return false;
        }

        s_modelShaderBindings[modelName] = shaderName;
        return true;
    }

    std::string AssetLoader::getModelShader(const std::string& modelName) {
        auto binding = s_modelShaderBindings.find(modelName);
        if (binding != s_modelShaderBindings.end()) {
            return binding->second;
        }

        auto fallbackShader = s_shaders.find("Blinn-Phong");
        if (fallbackShader != s_shaders.end()) {
            return "Blinn-Phong";
        }

        if (!s_shaders.empty()) {
            return s_shaders.begin()->first;
        }

        return {};
    }

    void AssetLoader::setActiveModel(const std::string& modelName) {
        if (s_models.find(modelName) == s_models.end()) {
            return;
        }

        s_activeModelName = modelName;
    }

    std::string AssetLoader::getActiveModel() {
        return s_activeModelName;
    }

    void AssetLoader::loadDefaultAssets() {
        loadTexture2D("Checker2D", "assets/textures/checker.ppm");
        loadTexture3D("CheckerVolume", "assets/textures/checker.ppm", 4);

        loadShader("Textured", "assets/shaders/textured.vert", "assets/shaders/textured.frag");
        loadShader("Blinn-Phong", "assets/shaders/blinn_phong.vert", "assets/shaders/blinn_phong.frag");
        loadShader("PBR", "assets/shaders/pbr.vert", "assets/shaders/pbr.frag");
        loadComputeShader("DefaultCompute", "assets/shaders/default_compute.comp");

        loadModel("Test Model", "assets/models/test_cube.obj");
        loadModel("Teapot", "assets/models/teapot.obj");

        if (s_models.find("Test Model") != s_models.end()) {
            s_activeModelName = "Test Model";
            setModelShader("Test Model", "Blinn-Phong");
        }
        if (s_models.find("Teapot") != s_models.end()) {
            s_activeModelName = "Teapot";
            setModelShader("Teapot", "Blinn-Phong");
        }
    }

}
