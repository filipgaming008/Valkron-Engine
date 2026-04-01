#pragma once

#include "Core/Core.hpp"
#include "Renderer/Camera.hpp"

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

    class VALKRON_API Scene {
        public:
            explicit Scene(std::string name = "Untitled Scene");

            const std::string& getName() const;
            void setName(std::string name);

            SceneState getState() const;
            void setState(SceneState state);

            void addEntity(std::string entityName);
            bool removeEntity(const std::string& entityName);
            const std::vector<std::string>& getEntities() const;

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

            std::vector<std::string> m_entities;
            std::vector<SceneAsset> m_assets;
            std::unordered_map<std::string, std::string> m_settings;
            std::unordered_map<std::string, std::string> m_gameState;
            std::vector<SceneScript> m_scripts;
            std::vector<SceneCamera> m_cameras;
    };

}
