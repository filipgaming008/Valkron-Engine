#pragma once

#include "Core/Core.hpp"
#include "Engine/Scene.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace Valkron {

    enum class RuntimeImportMode {
        Auto,
        Texture2D,
        Texture3D,
        Shader,
        Compute,
        Model
    };

    struct RuntimeAssetImportResult {
        bool success = false;
        std::vector<std::string> messages;
    };

    class VALKRON_API RuntimeAssetImportService {
        public:
            static bool isPathAllowedForMode(const std::filesystem::path& path, RuntimeImportMode mode);
            static RuntimeAssetImportResult importAsset(Scene& scene, const std::string& assetPath, RuntimeImportMode mode, int texture3DDepth);
    };

}
