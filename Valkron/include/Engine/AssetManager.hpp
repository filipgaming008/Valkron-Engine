#pragma once

#include "Core/Core.hpp"
#include "Engine/Scene.hpp"

#include <filesystem>
#include <string>

namespace Valkron {

    class VALKRON_API AssetManager {
        public:
            static std::filesystem::path getDefaultCachePath();
            static void setCachePath(const std::filesystem::path& cachePath);
            static const std::filesystem::path& getCachePath();

            static bool saveSceneAssetCache(const Scene& scene);
            static bool loadSceneAssetCache(Scene& scene);

        private:
            static std::filesystem::path s_cachePath;
    };

}
