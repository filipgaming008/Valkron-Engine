#pragma once

#include "Core/Core.hpp"

#include <filesystem>
#include <string>

namespace Valkron {

struct VALKRON_API EngineSettings {
    bool windowUseDesktopResolution = false;
    int windowWidth = 1280;
    int windowHeight = 720;
    float uiScale = 1.0f;
};

class VALKRON_API Settings {
public:
    static EngineSettings load();

private:
    static void applySetting(EngineSettings& settings, const std::string& key, const std::string& value);
    static std::filesystem::path resolveSettingsPath();
};

} // namespace Valkron
