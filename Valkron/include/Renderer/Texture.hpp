#pragma once

#include "Core/Core.hpp"

#include <string>

namespace Valkron {

class VALKRON_API Texture {
public:
    Texture();
    ~Texture();

    bool loadTexture(const std::string& path, bool flipVertical = true);
    void createEmpty(int width, int height, unsigned int internalFormat, unsigned int format, unsigned int type);
    void bind(unsigned int slot = 0) const;
    void unbind() const;

    unsigned int getID() const {
        return m_textureID;
    }
    int getWidth() const {
        return m_width;
    }
    int getHeight() const {
        return m_height;
    }

private:
    unsigned int m_textureID = 0;
    int m_width = 0;
    int m_height = 0;
    int m_channels = 0;
};

} // namespace Valkron