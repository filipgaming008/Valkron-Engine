#include "Application/UI/Panels/UILayerPanelsInternal.hpp"

#include <filesystem>

namespace Valkron {

    std::string deriveAssetBaseName(const std::string& absoluteOrRelativePath, const std::string& fallbackName) {
        const std::filesystem::path filePath(absoluteOrRelativePath);
        const std::string stem = filePath.stem().string();
        return stem.empty() ? fallbackName : stem;
    }

    void ensureEntityUsesPbrComponent(SceneEntity& entity) {
        if (entity.type != SceneEntityType::Generic || entity.modelAssetName.empty()) {
            return;
        }

        const bool shouldPreferPbr =
            !entity.shaderComponent.enabled ||
            entity.shaderComponent.shaderName.empty() ||
            entity.shaderComponent.shaderName == "Blinn-Phong";

        entity.shaderComponent.enabled = true;

        if (!shouldPreferPbr) {
            return;
        }

        if (AssetLoader::getShader("PBR") != nullptr) {
            entity.shaderComponent.shaderName = "PBR";
            return;
        }

        const std::vector<std::string> shaderNames = AssetLoader::getShaderNames();
        if (!shaderNames.empty()) {
            entity.shaderComponent.shaderName = shaderNames.front();
        }
    }

}
