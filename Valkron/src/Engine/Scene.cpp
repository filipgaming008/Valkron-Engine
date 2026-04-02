#include "Engine/Scene.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
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

    void Scene::addEntity(std::string entityName, SceneEntityType type) {
        const std::string uniqueName = makeUniqueEntityName(entityName);
        m_entities.push_back(SceneEntity{uniqueName, SceneTransform{}, -1, type});
        m_entityNameCacheDirty = true;
    }

    bool Scene::renameEntity(const std::string& oldName, std::string newName) {
        if (newName.empty()) {
            return false;
        }

        const std::optional<std::size_t> entityIndex = findEntityIndex(oldName);
        if (!entityIndex.has_value()) {
            return false;
        }

        const std::optional<std::size_t> conflictingEntity = findEntityIndex(newName);
        if (conflictingEntity.has_value() && conflictingEntity.value() != entityIndex.value()) {
            return false;
        }

        m_entities[entityIndex.value()].name = std::move(newName);
        m_entityNameCacheDirty = true;
        return true;
    }

    bool Scene::removeEntity(const std::string& entityName) {
        const std::optional<std::size_t> entityIndex = findEntityIndex(entityName);
        if (!entityIndex.has_value()) {
            return false;
        }

        const int removedIndex = static_cast<int>(entityIndex.value());
        m_entities.erase(m_entities.begin() + static_cast<std::ptrdiff_t>(removedIndex));

        for (SceneEntity& entity : m_entities) {
            if (entity.parentIndex == removedIndex) {
                entity.parentIndex = -1;
                continue;
            }

            if (entity.parentIndex > removedIndex) {
                --entity.parentIndex;
            }
        }

        m_entityNameCacheDirty = true;
        return true;
    }

    const std::vector<std::string>& Scene::getEntities() const {
        if (m_entityNameCacheDirty) {
            refreshEntityNameCache();
        }

        return m_entityNameCache;
    }

    const std::vector<SceneEntity>& Scene::getEntityData() const {
        return m_entities;
    }

    std::optional<std::size_t> Scene::findEntityIndex(const std::string& entityName) const {
        const auto it = std::find_if(m_entities.begin(), m_entities.end(), [&entityName](const SceneEntity& entity) {
            return entity.name == entityName;
        });

        if (it == m_entities.end()) {
            return std::nullopt;
        }

        return static_cast<std::size_t>(std::distance(m_entities.begin(), it));
    }

    SceneEntity* Scene::getEntityByIndex(std::size_t index) {
        if (index >= m_entities.size()) {
            return nullptr;
        }

        return &m_entities[index];
    }

    const SceneEntity* Scene::getEntityByIndex(std::size_t index) const {
        if (index >= m_entities.size()) {
            return nullptr;
        }

        return &m_entities[index];
    }

    bool Scene::setEntityParent(std::size_t entityIndex, std::optional<std::size_t> parentIndex) {
        if (entityIndex >= m_entities.size()) {
            return false;
        }

        if (!parentIndex.has_value()) {
            m_entities[entityIndex].parentIndex = -1;
            return true;
        }

        if (parentIndex.value() >= m_entities.size() || parentIndex.value() == entityIndex) {
            return false;
        }

        if (wouldCreateParentingCycle(entityIndex, parentIndex.value())) {
            return false;
        }

        m_entities[entityIndex].parentIndex = static_cast<int>(parentIndex.value());
        return true;
    }

    bool Scene::setEntityParentByName(const std::string& entityName, std::optional<std::string> parentName) {
        const std::optional<std::size_t> entityIndex = findEntityIndex(entityName);
        if (!entityIndex.has_value()) {
            return false;
        }

        if (!parentName.has_value() || parentName->empty()) {
            return setEntityParent(entityIndex.value(), std::nullopt);
        }

        const std::optional<std::size_t> parentIndex = findEntityIndex(parentName.value());
        if (!parentIndex.has_value()) {
            return false;
        }

        return setEntityParent(entityIndex.value(), parentIndex.value());
    }

    std::string Scene::makeUniqueEntityName(const std::string& baseName) const {
        std::string resolvedBaseName = baseName;
        if (resolvedBaseName.empty()) {
            resolvedBaseName = "Entity";
        }

        if (!findEntityIndex(resolvedBaseName).has_value()) {
            return resolvedBaseName;
        }

        int suffix = 1;
        for (;;) {
            const std::string candidateName = resolvedBaseName + "_" + std::to_string(suffix);
            if (!findEntityIndex(candidateName).has_value()) {
                return candidateName;
            }

            ++suffix;
        }
    }

    bool Scene::wouldCreateParentingCycle(std::size_t entityIndex, std::size_t parentIndex) const {
        int currentParentIndex = static_cast<int>(parentIndex);
        while (currentParentIndex >= 0 && currentParentIndex < static_cast<int>(m_entities.size())) {
            if (currentParentIndex == static_cast<int>(entityIndex)) {
                return true;
            }

            currentParentIndex = m_entities[static_cast<std::size_t>(currentParentIndex)].parentIndex;
        }

        return false;
    }

    void Scene::refreshEntityNameCache() const {
        m_entityNameCache.clear();
        m_entityNameCache.reserve(m_entities.size());
        for (const SceneEntity& entity : m_entities) {
            m_entityNameCache.push_back(entity.name);
        }

        m_entityNameCacheDirty = false;
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
