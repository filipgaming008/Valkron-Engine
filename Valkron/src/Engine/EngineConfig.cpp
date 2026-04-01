#include "Engine/EngineConfig.hpp"

#include "Core/FileSystem.hpp"
#include "Core/Log.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

namespace Valkron {

    std::string trim(const std::string& value) {
        std::size_t start = 0;
        while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
            ++start;
        }

        std::size_t end = value.size();
        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
            --end;
        }

        return value.substr(start, end - start);
    }

    bool parseBool(const std::string& value, bool fallbackValue) {
        std::string normalized = trim(value);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on") {
            return true;
        }

        if (normalized == "false" || normalized == "0" || normalized == "no" || normalized == "off") {
            return false;
        }

        return fallbackValue;
    }

    int parseInt(const std::string& value, int fallbackValue) {
        try {
            return std::stoi(trim(value));
        } catch (...) {
            return fallbackValue;
        }
    }

    EngineConfig::EngineConfig()
        : m_configPath(getDefaultConfigPath()) {}

    bool EngineConfig::load() {
        m_configPath = getDefaultConfigPath();

        std::ifstream input(m_configPath);
        if (!input.is_open()) {
            LOG_WARN("Engine config not found at " + m_configPath.string() + ". Creating defaults.");
            return save();
        }

        EngineSettings loadedSettings = m_settings;

        std::string line;
        while (std::getline(input, line)) {
            std::string sanitizedLine = trim(line);
            if (sanitizedLine.empty() || sanitizedLine[0] == '#') {
                continue;
            }

            const std::size_t separatorPosition = sanitizedLine.find('=');
            if (separatorPosition == std::string::npos) {
                continue;
            }

            const std::string key = trim(sanitizedLine.substr(0, separatorPosition));
            const std::string value = trim(sanitizedLine.substr(separatorPosition + 1));

            if (key == "window.width") {
                loadedSettings.windowWidth = parseInt(value, loadedSettings.windowWidth);
            } else if (key == "window.height") {
                loadedSettings.windowHeight = parseInt(value, loadedSettings.windowHeight);
            } else if (key == "window.fullscreen") {
                loadedSettings.fullscreen = parseBool(value, loadedSettings.fullscreen);
            } else if (key == "window.docked_fullscreen_windowed") {
                loadedSettings.dockedFullscreenWindowed = parseBool(value, loadedSettings.dockedFullscreenWindowed);
            } else if (key == "window.auto_detect_monitor_size") {
                loadedSettings.autoDetectMonitorSize = parseBool(value, loadedSettings.autoDetectMonitorSize);
            } else if (key == "window.title") {
                loadedSettings.windowTitle = value.empty() ? loadedSettings.windowTitle : value;
            }
        }

        loadedSettings.windowWidth = std::max(64, loadedSettings.windowWidth);
        loadedSettings.windowHeight = std::max(64, loadedSettings.windowHeight);
        if (loadedSettings.windowTitle.empty()) {
            loadedSettings.windowTitle = "Valkron Engine";
        }

        m_settings = loadedSettings;
        LOG_INFO("Loaded engine config from " + m_configPath.string());
        return true;
    }

    bool EngineConfig::save() const {
        std::error_code errorCode;
        std::filesystem::create_directories(m_configPath.parent_path(), errorCode);

        std::ofstream output(m_configPath, std::ios::trunc);
        if (!output.is_open()) {
            LOG_ERROR("Failed to write engine config to " + m_configPath.string());
            return false;
        }

        output << "# Valkron Engine configuration\n";
        output << "# This file is loaded on startup and can be edited in the in-engine UI.\n\n";
        output << "window.width=" << m_settings.windowWidth << "\n";
        output << "window.height=" << m_settings.windowHeight << "\n";
        output << "window.fullscreen=" << (m_settings.fullscreen ? "true" : "false") << "\n";
        output << "window.docked_fullscreen_windowed=" << (m_settings.dockedFullscreenWindowed ? "true" : "false") << "\n";
        output << "window.auto_detect_monitor_size=" << (m_settings.autoDetectMonitorSize ? "true" : "false") << "\n";
        output << "window.title=" << m_settings.windowTitle << "\n";

        LOG_INFO("Saved engine config to " + m_configPath.string());
        return true;
    }

    const EngineSettings& EngineConfig::getSettings() const {
        return m_settings;
    }

    void EngineConfig::setSettings(const EngineSettings& settings) {
        m_settings = settings;
        m_settings.windowWidth = std::max(64, m_settings.windowWidth);
        m_settings.windowHeight = std::max(64, m_settings.windowHeight);
        if (m_settings.windowTitle.empty()) {
            m_settings.windowTitle = "Valkron Engine";
        }
    }

    const std::filesystem::path& EngineConfig::getConfigPath() const {
        return m_configPath;
    }

    std::filesystem::path EngineConfig::getDefaultConfigPath() {
        return FileSystem::resolveAssetPath("config/engine.config");
    }

}
