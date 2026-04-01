#include "Renderer/Texture.hpp"

#include "Core/FileSystem.hpp"
#include "Core/Log.hpp"
#include "glad/gl.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Valkron {

    static bool loadAsciiPpmP3(const std::string& path, int& width, int& height, int& channels, std::vector<unsigned char>& outPixels, bool flipVertical) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }

        std::vector<std::string> tokens;
        std::string line;
        while (std::getline(file, line)) {
            const auto commentPos = line.find('#');
            if (commentPos != std::string::npos) {
                line = line.substr(0, commentPos);
            }

            std::istringstream lineStream(line);
            std::string token;
            while (lineStream >> token) {
                tokens.push_back(token);
            }
        }

        if (tokens.size() < 4 || tokens[0] != "P3") {
            return false;
        }

        std::size_t index = 1;
        width = std::stoi(tokens[index++]);
        height = std::stoi(tokens[index++]);
        const int maxValue = std::stoi(tokens[index++]);

        if (width <= 0 || height <= 0 || maxValue <= 0) {
            return false;
        }

        channels = 3;
        const std::size_t expectedValues = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3;
        if (tokens.size() - index < expectedValues) {
            return false;
        }

        outPixels.resize(expectedValues);
        for (std::size_t i = 0; i < expectedValues; ++i) {
            const int value = std::stoi(tokens[index + i]);
            const int scaled = static_cast<int>((static_cast<float>(value) / static_cast<float>(maxValue)) * 255.0f);
            outPixels[i] = static_cast<unsigned char>(std::clamp(scaled, 0, 255));
        }

        if (flipVertical) {
            const std::size_t rowBytes = static_cast<std::size_t>(width) * 3;
            std::vector<unsigned char> tempRow(rowBytes);
            for (int y = 0; y < height / 2; ++y) {
                unsigned char* top = outPixels.data() + static_cast<std::size_t>(y) * rowBytes;
                unsigned char* bottom = outPixels.data() + static_cast<std::size_t>(height - 1 - y) * rowBytes;
                std::copy(top, top + rowBytes, tempRow.begin());
                std::copy(bottom, bottom + rowBytes, top);
                std::copy(tempRow.begin(), tempRow.end(), bottom);
            }
        }

        return true;
    }

    Texture::Texture() {
        glGenTextures(1, &m_textureID);
        VALKRON_CORE_ASSERT(m_textureID != 0, "Failed to generate OpenGL texture ID");
    }

    Texture::~Texture() {
        if (m_textureID != 0) {
            glDeleteTextures(1, &m_textureID);
            m_textureID = 0;
        }
    }

    bool Texture::loadTexture(const std::string& path, bool flipVertical) {
        VALKRON_CORE_ASSERT(m_textureID != 0, "Texture::loadTexture called on invalid texture object");
        VALKRON_CORE_ASSERT(!path.empty(), "Texture path must not be empty");

        const std::filesystem::path resolvedPath = FileSystem::resolveExistingPath(path);
        const std::string resolvedPathString = resolvedPath.string();

        if (resolvedPathString.empty()) {
            LOG_ERROR("Resolved texture path is empty for input: " + path);
            return false;
        }

        stbi_set_flip_vertically_on_load(flipVertical ? 1 : 0);

        m_textureTarget = GL_TEXTURE_2D;
        m_is3DTexture = false;
        m_depth = 1;

        bind();
        glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(m_textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        unsigned char* data = stbi_load(resolvedPathString.c_str(), &m_width, &m_height, &m_channels, 0);
        if (!data) {
            std::vector<unsigned char> ppmPixels;
            if (!loadAsciiPpmP3(resolvedPathString, m_width, m_height, m_channels, ppmPixels, flipVertical)) {
                LOG_ERROR("Failed to load texture: " + resolvedPathString);
                return false;
            }

            glTexImage2D(m_textureTarget, 0, GL_RGB, m_width, m_height, 0, GL_RGB, GL_UNSIGNED_BYTE, ppmPixels.data());
            glGenerateMipmap(m_textureTarget);
            LOG_INFO("Loaded texture via PPM fallback: " + resolvedPathString);
            return true;
        }

        GLenum format = GL_RGB;
        if (m_channels == 1) {
            format = GL_RED;
        } else if (m_channels == 3) {
            format = GL_RGB;
        } else if (m_channels == 4) {
            format = GL_RGBA;
        }

        glTexImage2D(m_textureTarget, 0, static_cast<GLint>(format), m_width, m_height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(m_textureTarget);

        stbi_image_free(data);
        return true;
    }

    bool Texture::loadTexture3D(const std::string& path, int depth, bool flipVertical) {
        VALKRON_CORE_ASSERT(m_textureID != 0, "Texture::loadTexture3D called on invalid texture object");
        VALKRON_CORE_ASSERT(!path.empty(), "Texture path must not be empty");

        const std::filesystem::path resolvedPath = FileSystem::resolveExistingPath(path);
        const std::string resolvedPathString = resolvedPath.string();
        if (resolvedPathString.empty()) {
            LOG_ERROR("Resolved texture path is empty for input: " + path);
            return false;
        }

        m_textureTarget = GL_TEXTURE_3D;
        m_is3DTexture = true;
        m_depth = std::max(1, depth);

        stbi_set_flip_vertically_on_load(flipVertical ? 1 : 0);

        bind();
        glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_R, GL_REPEAT);
        glTexParameteri(m_textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        std::vector<unsigned char> sourcePixels;
        unsigned char* stbData = stbi_load(resolvedPathString.c_str(), &m_width, &m_height, &m_channels, 0);
        if (stbData != nullptr) {
            const std::size_t sliceSize = static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height) * static_cast<std::size_t>(m_channels);
            sourcePixels.assign(stbData, stbData + sliceSize);
            stbi_image_free(stbData);
        } else {
            if (!loadAsciiPpmP3(resolvedPathString, m_width, m_height, m_channels, sourcePixels, flipVertical)) {
                LOG_ERROR("Failed to load 3D texture source image: " + resolvedPathString);
                return false;
            }
        }

        GLenum format = GL_RGB;
        if (m_channels == 1) {
            format = GL_RED;
        } else if (m_channels == 3) {
            format = GL_RGB;
        } else if (m_channels == 4) {
            format = GL_RGBA;
        }

        const std::size_t sliceSize = static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height) * static_cast<std::size_t>(m_channels);
        std::vector<unsigned char> volumeData(sliceSize * static_cast<std::size_t>(m_depth));
        for (int z = 0; z < m_depth; ++z) {
            std::copy(sourcePixels.begin(), sourcePixels.end(), volumeData.begin() + static_cast<std::ptrdiff_t>(z) * static_cast<std::ptrdiff_t>(sliceSize));
        }

        glTexImage3D(
            m_textureTarget,
            0,
            static_cast<GLint>(format),
            m_width,
            m_height,
            m_depth,
            0,
            format,
            GL_UNSIGNED_BYTE,
            volumeData.data()
        );

        LOG_INFO("Loaded 3D texture (depth=" + std::to_string(m_depth) + "): " + resolvedPathString);
        return true;
    }

    void Texture::createEmpty(int width, int height, unsigned int internalFormat, unsigned int format, unsigned int type) {
        VALKRON_CORE_ASSERT(m_textureID != 0, "Texture::createEmpty called on invalid texture object");
        VALKRON_CORE_ASSERT(width > 0 && height > 0, "Texture dimensions must be positive");

        m_width = width;
        m_height = height;
        m_depth = 1;
        m_textureTarget = GL_TEXTURE_2D;
        m_is3DTexture = false;

        bind();
        glTexParameteri(m_textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(m_textureTarget, 0, static_cast<GLint>(internalFormat), width, height, 0, format, type, nullptr);
    }

    void Texture::bind(unsigned int slot) const {
        VALKRON_CORE_ASSERT(m_textureID != 0, "Attempted to bind invalid texture object");
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(m_textureTarget, m_textureID);
    }

    void Texture::unbind() const {
        glBindTexture(m_textureTarget, 0);
    }

}