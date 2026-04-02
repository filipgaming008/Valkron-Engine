#pragma once

#include "Core/Core.hpp"
#include "Renderer/Camera.hpp"

#include "glm/vec3.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Valkron {

    enum class SceneState {
        Edit,
        Play,
        Pause
    };

    struct VALKRON_API SceneAsset {
        std::string name;
        std::string path;
    };

    struct VALKRON_API SceneScript {
        std::string name;
        std::string path;
        bool enabled = true;
    };

    struct VALKRON_API SceneCamera {
        std::string name;
        Camera camera;
        bool primary = false;
    };

    enum class SceneEntityType {
        Generic,
        Camera,
        Light
    };

    struct VALKRON_API SceneTransform {
        glm::vec3 position{0.0f, 0.0f, 0.0f};
        glm::vec3 rotation{0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f, 1.0f, 1.0f};
        glm::vec3 size{1.0f, 1.0f, 1.0f};
    };

    struct VALKRON_API SceneEntity {
        std::string name;
        SceneTransform transform{};
        int parentIndex = -1;
        SceneEntityType type = SceneEntityType::Generic;
        std::string modelAssetName;
    };

    class VALKRON_API Scene {
        public:
            explicit Scene(std::string name = "Untitled Scene");

            const std::string& getName() const;
            void setName(std::string name);

            SceneState getState() const;
            void setState(SceneState state);

            void addEntity(std::string entityName, SceneEntityType type = SceneEntityType::Generic);
            bool renameEntity(const std::string& oldName, std::string newName);
            bool removeEntity(const std::string& entityName);
            const std::vector<std::string>& getEntities() const;
            const std::vector<SceneEntity>& getEntityData() const;
            std::optional<std::size_t> findEntityIndex(const std::string& entityName) const;
            SceneEntity* getEntityByIndex(std::size_t index);
            const SceneEntity* getEntityByIndex(std::size_t index) const;
            bool setEntityParent(std::size_t entityIndex, std::optional<std::size_t> parentIndex);
            bool setEntityParentByName(const std::string& entityName, std::optional<std::string> parentName);
            std::string makeUniqueEntityName(const std::string& baseName) const;

            void addAsset(std::string name, std::string path);
            bool removeAsset(const std::string& name);
            const std::vector<SceneAsset>& getAssets() const;

            void setSetting(std::string key, std::string value);
            std::optional<std::string> getSetting(const std::string& key) const;
            const std::unordered_map<std::string, std::string>& getSettings() const;

            void setGameStateValue(std::string key, std::string value);
            std::optional<std::string> getGameStateValue(const std::string& key) const;
            const std::unordered_map<std::string, std::string>& getGameState() const;

            void addScript(std::string name, std::string path, bool enabled = true);
            bool removeScript(const std::string& name);
            bool setScriptEnabled(const std::string& name, bool enabled);
            const std::vector<SceneScript>& getScripts() const;

            void addCamera(std::string name, CameraType type = CameraType::Perspective, bool primary = false);
            bool removeCamera(const std::string& name);
            bool setPrimaryCamera(const std::string& name);
            const std::vector<SceneCamera>& getCameras() const;

        private:
            std::string m_name;
            SceneState m_state = SceneState::Edit;

            bool wouldCreateParentingCycle(std::size_t entityIndex, std::size_t parentIndex) const;
            void refreshEntityNameCache() const;

            std::vector<SceneEntity> m_entities;
            mutable std::vector<std::string> m_entityNameCache;
            mutable bool m_entityNameCacheDirty = true;
            std::vector<SceneAsset> m_assets;
            std::unordered_map<std::string, std::string> m_settings;
            std::unordered_map<std::string, std::string> m_gameState;
            std::vector<SceneScript> m_scripts;
            std::vector<SceneCamera> m_cameras;
    };

}
