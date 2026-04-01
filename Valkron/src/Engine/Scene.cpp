#include "Engine/Scene.hpp"

#include <algorithm>
#include <utility>

namespace Valkron {

    Scene::Scene(std::string name)
        : m_name(std::move(name)) {}

    const std::string& Scene::getName() const {
        return m_name;
    }

    void Scene::setName(std::string name) {
        m_name = std::move(name);
    }

    SceneState Scene::getState() const {
        return m_state;
    }

    void Scene::setState(SceneState state) {
        m_state = state;
    }

    void Scene::addEntity(std::string entityName) {
        if (entityName.empty()) {
            return;
        }

        const auto it = std::find(m_entities.begin(), m_entities.end(), entityName);
        if (it == m_entities.end()) {
            m_entities.push_back(std::move(entityName));
        }
    }

    bool Scene::removeEntity(const std::string& entityName) {
        const auto it = std::find(m_entities.begin(), m_entities.end(), entityName);
        if (it == m_entities.end()) {
            return false;
        }

        m_entities.erase(it);
        return true;
    }

    const std::vector<std::string>& Scene::getEntities() const {
        return m_entities;
    }

    void Scene::addAsset(std::string name, std::string path) {
        if (name.empty()) {
            return;
        }

        const auto it = std::find_if(m_assets.begin(), m_assets.end(), [&name](const SceneAsset& asset) {
            return asset.name == name;
        });

        if (it != m_assets.end()) {
            it->path = std::move(path);
            return;
        }

        m_assets.push_back(SceneAsset{std::move(name), std::move(path)});
    }

    bool Scene::removeAsset(const std::string& name) {
        const auto it = std::find_if(m_assets.begin(), m_assets.end(), [&name](const SceneAsset& asset) {
            return asset.name == name;
        });

        if (it == m_assets.end()) {
            return false;
        }

        m_assets.erase(it);
        return true;
    }

    const std::vector<SceneAsset>& Scene::getAssets() const {
        return m_assets;
    }

    void Scene::setSetting(std::string key, std::string value) {
        if (key.empty()) {
            return;
        }

        m_settings[std::move(key)] = std::move(value);
    }

    std::optional<std::string> Scene::getSetting(const std::string& key) const {
        const auto it = m_settings.find(key);
        if (it == m_settings.end()) {
            return std::nullopt;
        }

        return it->second;
    }

    const std::unordered_map<std::string, std::string>& Scene::getSettings() const {
        return m_settings;
    }

    void Scene::setGameStateValue(std::string key, std::string value) {
        if (key.empty()) {
            return;
        }

        m_gameState[std::move(key)] = std::move(value);
    }

    std::optional<std::string> Scene::getGameStateValue(const std::string& key) const {
        const auto it = m_gameState.find(key);
        if (it == m_gameState.end()) {
            return std::nullopt;
        }

        return it->second;
    }

    const std::unordered_map<std::string, std::string>& Scene::getGameState() const {
        return m_gameState;
    }

    void Scene::addScript(std::string name, std::string path, bool enabled) {
        if (name.empty()) {
            return;
        }

        const auto it = std::find_if(m_scripts.begin(), m_scripts.end(), [&name](const SceneScript& script) {
            return script.name == name;
        });

        if (it != m_scripts.end()) {
            it->path = std::move(path);
            it->enabled = enabled;
            return;
        }

        m_scripts.push_back(SceneScript{std::move(name), std::move(path), enabled});
    }

    bool Scene::removeScript(const std::string& name) {
        const auto it = std::find_if(m_scripts.begin(), m_scripts.end(), [&name](const SceneScript& script) {
            return script.name == name;
        });

        if (it == m_scripts.end()) {
            return false;
        }

        m_scripts.erase(it);
        return true;
    }

    bool Scene::setScriptEnabled(const std::string& name, bool enabled) {
        const auto it = std::find_if(m_scripts.begin(), m_scripts.end(), [&name](const SceneScript& script) {
            return script.name == name;
        });

        if (it == m_scripts.end()) {
            return false;
        }

        it->enabled = enabled;
        return true;
    }

    const std::vector<SceneScript>& Scene::getScripts() const {
        return m_scripts;
    }

    void Scene::addCamera(std::string name, CameraType type, bool primary) {
        if (name.empty()) {
            return;
        }

        const auto it = std::find_if(m_cameras.begin(), m_cameras.end(), [&name](const SceneCamera& camera) {
            return camera.name == name;
        });

        if (it != m_cameras.end()) {
            it->camera.setType(type);
            if (primary) {
                setPrimaryCamera(name);
            }
            return;
        }

        m_cameras.push_back(SceneCamera{std::move(name), Camera(type), primary});

        if (primary || m_cameras.size() == 1) {
            setPrimaryCamera(m_cameras.back().name);
        }
    }

    bool Scene::removeCamera(const std::string& name) {
        const auto it = std::find_if(m_cameras.begin(), m_cameras.end(), [&name](const SceneCamera& camera) {
            return camera.name == name;
        });

        if (it == m_cameras.end()) {
            return false;
        }

        const bool removedPrimary = it->primary;
        m_cameras.erase(it);

        if (removedPrimary && !m_cameras.empty()) {
            m_cameras.front().primary = true;
        }

        return true;
    }

    bool Scene::setPrimaryCamera(const std::string& name) {
        bool found = false;
        for (SceneCamera& camera : m_cameras) {
            const bool isPrimary = (camera.name == name);
            camera.primary = isPrimary;
            found = found || isPrimary;
        }

        return found;
    }

    const std::vector<SceneCamera>& Scene::getCameras() const {
        return m_cameras;
    }

}
