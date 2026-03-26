#include "Renderer/Buffers.hpp"
#include "glad/gl.h"

namespace Valkron {

    unsigned int VertexBufferElement::getSizeOfType(unsigned int type) {
        switch (type) {
            case GL_FLOAT: return 4;
            case GL_UNSIGNED_INT: return 4;
            case GL_UNSIGNED_BYTE: return 1;
            default: return 0;
        }
    }

    VertexBuffer::VertexBuffer(const void* data, unsigned int size) {
        glGenBuffers(1, &m_rendererID);
        glBindBuffer(GL_ARRAY_BUFFER, m_rendererID);
        glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
    }

    VertexBuffer::~VertexBuffer() {
        glDeleteBuffers(1, &m_rendererID);
    }

    void VertexBuffer::bind() const {
        glBindBuffer(GL_ARRAY_BUFFER, m_rendererID);
    }

    void VertexBuffer::unbind() const {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    IndexBuffer::IndexBuffer(const unsigned int* data, unsigned int count)
        : m_count(count) {
        glGenBuffers(1, &m_rendererID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_rendererID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(count * sizeof(unsigned int)), data, GL_STATIC_DRAW);
    }

    IndexBuffer::~IndexBuffer() {
        glDeleteBuffers(1, &m_rendererID);
    }

    void IndexBuffer::bind() const {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_rendererID);
    }

    void IndexBuffer::unbind() const {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    VertexArray::VertexArray() {
        glGenVertexArrays(1, &m_rendererID);
    }

    VertexArray::~VertexArray() {
        glDeleteVertexArrays(1, &m_rendererID);
    }

    void VertexArray::bind() const {
        glBindVertexArray(m_rendererID);
    }

    void VertexArray::unbind() const {
        glBindVertexArray(0);
    }

    void VertexArray::addBuffer(const VertexBuffer& vertexBuffer, const VertexLayout& layout) {
        bind();
        vertexBuffer.bind();

        const auto& elements = layout.getElements();
        std::size_t offset = 0;
        for (unsigned int i = 0; i < elements.size(); ++i) {
            const auto& element = elements[i];
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(
                i,
                static_cast<GLint>(element.count),
                element.type,
                element.normalized,
                static_cast<GLsizei>(layout.getStride()),
                reinterpret_cast<const void*>(offset)
            );

            offset += static_cast<std::size_t>(element.count) * VertexBufferElement::getSizeOfType(element.type);
        }
    }

    DepthBuffer::DepthBuffer() {
        glGenRenderbuffers(1, &m_rendererID);
    }

    DepthBuffer::~DepthBuffer() {
        glDeleteRenderbuffers(1, &m_rendererID);
    }

    void DepthBuffer::bind() const {
        glBindRenderbuffer(GL_RENDERBUFFER, m_rendererID);
    }

    void DepthBuffer::unbind() const {
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    void DepthBuffer::allocateStorage(int width, int height) {
        bind();
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        unbind();
    }

    FrameBuffer::FrameBuffer() {
        glGenFramebuffers(1, &m_rendererID);
    }

    FrameBuffer::~FrameBuffer() {
        glDeleteFramebuffers(1, &m_rendererID);
    }

    void FrameBuffer::bind(unsigned int target) const {
        glBindFramebuffer(target, m_rendererID);
    }

    void FrameBuffer::unbind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void FrameBuffer::attachColorTexture(unsigned int textureID, unsigned int attachment) const {
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, textureID, 0);
    }

    void FrameBuffer::attachDepthBuffer(unsigned int depthBufferID) const {
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthBufferID);
    }

    bool FrameBuffer::isComplete() const {
        return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    }

    void FrameBuffer::blitToDefault(int srcWidth, int srcHeight, int dstWidth, int dstHeight, unsigned int mask) const {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_rendererID);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, srcWidth, srcHeight, 0, 0, dstWidth, dstHeight, mask, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

}