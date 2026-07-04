#pragma once

#include "Core/Core.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Valkron {

    struct VALKRON_API VertexBufferElement {
        unsigned int type;
        unsigned int count;
        unsigned char normalized;

        static unsigned int getSizeOfType(unsigned int type) {
            switch (type) {
                case 0x1406:
                case 0x1405:
                    return 4;
                case 0x1401:
                    return 1;
                default:
                    return 0;
            }
        }
    };

    class VALKRON_API VertexLayout {
        public:
            template<typename T>
            void push(unsigned int count);

            const std::vector<VertexBufferElement>& getElements() const { return m_elements; }
            unsigned int getStride() const { return m_stride; }

        private:
            std::vector<VertexBufferElement> m_elements;
            unsigned int m_stride = 0;
    };

    template<>
    inline void VertexLayout::push<float>(unsigned int count) {
        m_elements.push_back({0x1406, count, 0});
        m_stride += count * VertexBufferElement::getSizeOfType(0x1406);
    }

    template<>
    inline void VertexLayout::push<unsigned int>(unsigned int count) {
        m_elements.push_back({0x1405, count, 0});
        m_stride += count * VertexBufferElement::getSizeOfType(0x1405);
    }

    template<>
    inline void VertexLayout::push<unsigned char>(unsigned int count) {
        m_elements.push_back({0x1401, count, 1});
        m_stride += count * VertexBufferElement::getSizeOfType(0x1401);
    }

    class VALKRON_API VertexBuffer {
        public:
            VertexBuffer(const void* data, unsigned int size);
            ~VertexBuffer();

            void bind() const;
            void unbind() const;

        private:
            unsigned int m_rendererID = 0;
    };

    class VALKRON_API IndexBuffer {
        public:
            IndexBuffer(const unsigned int* data, unsigned int count);
            ~IndexBuffer();

            void bind() const;
            void unbind() const;

            unsigned int getCount() const { return m_count; }
            unsigned int getID() const { return m_rendererID; }

        private:
            unsigned int m_rendererID = 0;
            unsigned int m_count = 0;
    };

    class VALKRON_API VertexArray {
        public:
            VertexArray();
            ~VertexArray();

            void bind() const;
            void unbind() const;
            void addBuffer(const VertexBuffer& vertexBuffer, const VertexLayout& layout);

            unsigned int getID() const { return m_rendererID; }

        private:
            unsigned int m_rendererID = 0;
    };

    class VALKRON_API DepthBuffer {
        public:
            DepthBuffer();
            ~DepthBuffer();

            void bind() const;
            void unbind() const;
            void allocateStorage(int width, int height);

            unsigned int getID() const { return m_rendererID; }

        private:
            unsigned int m_rendererID = 0;
    };

    class VALKRON_API FrameBuffer {
        public:
            FrameBuffer();
            ~FrameBuffer();

            void bind(unsigned int target = 0x8D40) const;
            void unbind() const;
            void attachColorTexture(unsigned int textureID, unsigned int attachment = 0x8CE0) const;
            void attachDepthBuffer(unsigned int depthBufferID) const;
            bool isComplete() const;
            void blitToDefault(int srcWidth, int srcHeight, int dstWidth, int dstHeight, unsigned int mask = 0x00004000) const;

            unsigned int getID() const { return m_rendererID; }

        private:
            unsigned int m_rendererID = 0;
    };

}