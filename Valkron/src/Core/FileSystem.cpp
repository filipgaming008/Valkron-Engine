#include "Core/FileSystem.hpp"

#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace Valkron::FileSystem {

    static std::filesystem::path stripAssetsPrefix(const std::filesystem::path& inputPath) {
        const std::string normalized = inputPath.lexically_normal().generic_string();
        constexpr const char* assetsPrefix = "assets/";

        if (normalized.rfind(assetsPrefix, 0) == 0) {
            return std::filesystem::path(normalized.substr(7));
        }

        return inputPath;
    }

    std::filesystem::path getExecutableDirectory() {
#if defined(_WIN32)
        std::wstring executablePath(MAX_PATH, L'\0');
        DWORD length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
        if (length == 0) {
            std::error_code errorCode;
            return std::filesystem::current_path(errorCode);
        }

        while (length == executablePath.size()) {
            executablePath.resize(executablePath.size() * 2);
            length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
            if (length == 0) {
                std::error_code errorCode;
                return std::filesystem::current_path(errorCode);
            }
        }

        executablePath.resize(length);
        const std::filesystem::path path(executablePath);
        return path.has_parent_path() ? path.parent_path() : path;
#elif defined(__APPLE__)
        std::uint32_t size = 0;
        if (_NSGetExecutablePath(nullptr, &size) != -1 || size == 0) {
            std::error_code errorCode;
            return std::filesystem::current_path(errorCode);
        }

        std::string buffer(size, '\0');
        if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
            std::error_code errorCode;
            return std::filesystem::current_path(errorCode);
        }

        const std::filesystem::path executablePath(buffer.c_str());
        return executablePath.has_parent_path() ? executablePath.parent_path() : executablePath;
#else
        std::error_code errorCode;
        const std::filesystem::path executablePath = std::filesystem::read_symlink("/proc/self/exe", errorCode);
        if (!errorCode && executablePath.has_parent_path()) {
            return executablePath.parent_path();
        }

        return std::filesystem::current_path(errorCode);
#endif
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
