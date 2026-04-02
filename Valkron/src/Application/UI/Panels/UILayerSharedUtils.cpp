#include "Application/UI/Panels/UILayerPanelsInternal.hpp"

#include <filesystem>

namespace Valkron {

    std::string deriveAssetBaseName(const std::string& absoluteOrRelativePath, const std::string& fallbackName) {
        const std::filesystem::path filePath(absoluteOrRelativePath);
        const std::string stem = filePath.stem().string();
        return stem.empty() ? fallbackName : stem;
    }

}
