#include "Core/Settings.hpp"

#include "Core/FileSystem.hpp"
#include "Core/Log.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

namespace Valkron {

namespace {
std::string trim(const std::string& input) {
    const auto first =
        std::find_if_not(input.begin(), input.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
    if (first == input.end()) {
        return "";
    }

    const auto last =
        std::find_if_not(input.rbegin(), input.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();

    return std::string(first, last);
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool parseBool(const std::string& value, bool fallback) {
    const std::string lowered = toLower(trim(value));
    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") {
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") {
        return false;
    }
    return fallback;
}
} // namespace

std::filesystem::path Settings::resolveSettingsPath() {
    const std::filesystem::path assetPath = FileSystem::resolveAssetPath("config/engine.cfg");
    if (std::filesystem::exists(assetPath)) {
        return assetPath;
    }

    return FileSystem::getExecutableDirectory() / "config" / "engine.cfg";
}

void Settings::applySetting(EngineSettings& settings, const std::string& key, const std::string& value) {
    if (key == "window.useDesktopResolution") {
        settings.windowUseDesktopResolution = parseBool(value, settings.windowUseDesktopResolution);
        return;
    }

    if (key == "window.width") {
        settings.windowWidth = std::max(320, std::stoi(value));
        return;
    }

    if (key == "window.height") {
        settings.windowHeight = std::max(240, std::stoi(value));
        return;
    }

    if (key == "ui.scale") {
        settings.uiScale = std::clamp(std::stof(value), 0.5f, 2.25f);
        return;
    }
}

EngineSettings Settings::load() {
    EngineSettings settings;
    const std::filesystem::path configPath = resolveSettingsPath();

    if (!std::filesystem::exists(configPath)) {
        LOG_WARN("Settings file not found, using defaults: " + configPath.string());
        return settings;
    }

    std::ifstream input(configPath);
    if (!input.is_open()) {
        LOG_WARN("Failed to open settings file, using defaults: " + configPath.string());
        return settings;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const std::string stripped = trim(line);
        if (stripped.empty() || stripped[0] == '#' || stripped[0] == ';') {
            continue;
        }

        const std::size_t delimiter = stripped.find('=');
        if (delimiter == std::string::npos) {
            LOG_WARN("Ignoring malformed setting at line " + std::to_string(lineNumber));
            continue;
        }

        const std::string key = trim(stripped.substr(0, delimiter));
        const std::string value = trim(stripped.substr(delimiter + 1));
        if (key.empty() || value.empty()) {
            continue;
        }

        try {
            applySetting(settings, key, value);
        } catch (const std::exception&) {
            LOG_WARN("Invalid value for setting '" + key + "' at line " + std::to_string(lineNumber));
        }
    }

    LOG_INFO("Loaded settings: windowUseDesktopResolution=" +
             std::string(settings.windowUseDesktopResolution ? "true" : "false") + ", windowWidth=" +
             std::to_string(settings.windowWidth) + ", windowHeight=" + std::to_string(settings.windowHeight) +
             ", uiScale=" + std::to_string(settings.uiScale));
    return settings;
}

} // namespace Valkron
