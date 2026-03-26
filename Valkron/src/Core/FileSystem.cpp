#include "Core/FileSystem.hpp"

#include <system_error>

namespace Valkron::FileSystem {

    namespace {
        std::filesystem::path stripAssetsPrefix(const std::filesystem::path& inputPath) {
            const std::string normalized = inputPath.lexically_normal().generic_string();
            constexpr const char* assetsPrefix = "assets/";

            if (normalized.rfind(assetsPrefix, 0) == 0) {
                return std::filesystem::path(normalized.substr(7));
            }

            return inputPath;
        }
    }

    std::filesystem::path getExecutableDirectory() {
        std::error_code errorCode;
        const std::filesystem::path executablePath = std::filesystem::read_symlink("/proc/self/exe", errorCode);
        if (!errorCode && executablePath.has_parent_path()) {
            return executablePath.parent_path();
        }

        return std::filesystem::current_path(errorCode);
    }

    std::filesystem::path getAssetRootDirectory() {
        return getExecutableDirectory() / "assets";
    }

    std::filesystem::path getAssetPath(const std::filesystem::path& relativeAssetPath) {
        return getAssetRootDirectory() / relativeAssetPath;
    }

    std::filesystem::path resolveAssetPath(const std::filesystem::path& assetRelativeOrAssetsPrefixedPath) {
        return getAssetPath(stripAssetsPrefix(assetRelativeOrAssetsPrefixedPath));
    }

    std::filesystem::path resolveExistingPath(const std::filesystem::path& path) {
        std::error_code errorCode;
        if (path.is_absolute() && std::filesystem::exists(path, errorCode)) {
            return path;
        }

        if (std::filesystem::exists(path, errorCode)) {
            return path;
        }

        const std::filesystem::path assetCandidate = resolveAssetPath(path);
        if (std::filesystem::exists(assetCandidate, errorCode)) {
            return assetCandidate;
        }

        return path;
    }

}
