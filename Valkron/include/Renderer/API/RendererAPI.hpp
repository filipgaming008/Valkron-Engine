#pragma once

#include "Core/Core.hpp"

#include "glad/gl.h"
#include "glm/glm.hpp"

#include <string>

namespace Valkron {

    enum class RendererAPIType {
        None = 0,
        OpenGL = 1
    };

    class VALKRON_API RendererAPI {
        public:
            virtual ~RendererAPI() = default;

            virtual void init() = 0;
            virtual void setViewport(int x, int y, int width, int height) = 0;
            virtual void setClearColor(float red, float green, float blue, float alpha) = 0;
            virtual void clear(bool clearColor = true, bool clearDepth = true) = 0;

            virtual void useShader(GLuint shaderID) = 0;
            virtual void bindVertexArray(GLuint vaoID) = 0;
            virtual void bindTexture(GLuint textureID, GLuint slot = 0) = 0;
            virtual void drawIndexed(GLuint indexBufferID, GLuint indexCount) = 0;

            virtual void setUniformMat4(GLuint shaderID, const std::string& name, const glm::mat4& matrix) = 0;
            virtual void setUniformVec4(GLuint shaderID, const std::string& name, const glm::vec4& vector) = 0;
            virtual void setUniformFloat(GLuint shaderID, const std::string& name, float value) = 0;
            virtual void setUniformInt(GLuint shaderID, const std::string& name, int value) = 0;

            virtual void setDepthTest(bool enabled) = 0;

            static RendererAPIType getAPI() { return s_api; }

        protected:
            static RendererAPIType s_api;
    };

}