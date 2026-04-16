#pragma once

#include "Core/Core.hpp"

#include <filesystem>
#include <string>

namespace Valkron {

    struct VALKRON_API EngineSettings {
        int windowWidth = 1280;
        int windowHeight = 720;
        bool fullscreen = false;
        bool dockedFullscreenWindowed = true;
        bool autoDetectMonitorSize = true;
        std::string windowTitle = "Valkron Engine";
        int deselectKey = 256;
        int deleteEntityKey = 261;
    };

    class VALKRON_API EngineConfig {
        public:
            EngineConfig();

            bool load();
            bool save() const;

            const EngineSettings& getSettings() const;
            void setSettings(const EngineSettings& settings);

            const std::filesystem::path& getConfigPath() const;
            static std::filesystem::path getDefaultConfigPath();

        private:
            EngineSettings m_settings;
            std::filesystem::path m_configPath;
    };

}
