#pragma once

#include "Core/Core.hpp"

#include <filesystem>

namespace Valkron::FileSystem {
    VALKRON_API std::filesystem::path getExecutableDirectory();
    VALKRON_API std::filesystem::path getAssetRootDirectory();
    VALKRON_API std::filesystem::path getAssetPath(const std::filesystem::path& relativeAssetPath);
    VALKRON_API std::filesystem::path resolveAssetPath(const std::filesystem::path& assetRelativeOrAssetsPrefixedPath);
    VALKRON_API std::filesystem::path resolveExistingPath(const std::filesystem::path& path);
}
