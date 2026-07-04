#pragma once

#include "Core/Core.hpp"
#include "Renderer/API/RendererAPI.hpp"
#include <memory>

namespace Valkron {

    class VALKRON_API RenderCommand {
        public:
            static void init();
            static void setViewport(int x, int y, int width, int height);
            static void setClearColor(float red, float green, float blue, float alpha);
            static void clear(bool clearColor = true, bool clearDepth = true);
            static void setDepthTest(bool enabled);

            static void useShader(GLuint shaderID);
            static void bindVertexArray(GLuint vaoID);
            static void bindTexture(GLuint textureID, GLuint slot = 0);
            static void drawIndexed(GLuint indexBufferID, GLuint indexCount);

            static void setUniformMat4(GLuint shaderID, const std::string& name, const glm::mat4& matrix);
            static void setUniformVec4(GLuint shaderID, const std::string& name, const glm::vec4& vector);
            static void setUniformFloat(GLuint shaderID, const std::string& name, float value);
            static void setUniformInt(GLuint shaderID, const std::string& name, int value);

        private:
            static std::unique_ptr<RendererAPI> s_rendererAPI;
    };

}