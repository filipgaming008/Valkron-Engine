#pragma once

#include "Renderer/API/RendererAPI.hpp"

namespace Valkron {

    class VALKRON_API OpenGLRendererAPI : public RendererAPI {
        public:
            void init() override;
            void setViewport(int x, int y, int width, int height) override;
            void setClearColor(float red, float green, float blue, float alpha) override;
            void clear(bool clearColor = true, bool clearDepth = true) override;

            void useShader(GLuint shaderID) override;
            void bindVertexArray(GLuint vaoID) override;
            void bindTexture(GLuint textureID, GLuint slot = 0) override;
            void drawIndexed(GLuint indexBufferID, GLuint indexCount) override;

            void setUniformMat4(GLuint shaderID, const std::string& name, const glm::mat4& matrix) override;
            void setUniformVec4(GLuint shaderID, const std::string& name, const glm::vec4& vector) override;
            void setUniformFloat(GLuint shaderID, const std::string& name, float value) override;
            void setUniformInt(GLuint shaderID, const std::string& name, int value) override;

            void setDepthTest(bool enabled) override;
    };

}