#pragma once

#include "Core/Core.hpp"

#include <string>

namespace Valkron {

    class VALKRON_API Texture {
        public:
            Texture();
            ~Texture();

            bool loadTexture(const std::string& path, bool flipVertical = true);
            bool loadTexture3D(const std::string& path, int depth = 1, bool flipVertical = true);
            void createEmpty(int width, int height, unsigned int internalFormat, unsigned int format, unsigned int type);
            void bind(unsigned int slot = 0) const;
            void unbind() const;

            unsigned int getID() const { return m_textureID; }
            int getWidth() const { return m_width; }
            int getHeight() const { return m_height; }
            int getDepth() const { return m_depth; }
            bool is3DTexture() const { return m_is3DTexture; }

        private:
            unsigned int m_textureID = 0;
            int m_width = 0;
            int m_height = 0;
            int m_depth = 1;
            int m_channels = 0;
            unsigned int m_textureTarget = 0x0DE1;
            bool m_is3DTexture = false;
    };

}