#include "Renderer/API/OpenGLRendererAPI.hpp"
#include "glad/gl.h"
#include "glm/gtc/type_ptr.hpp"

namespace Valkron {

    void OpenGLRendererAPI::init() {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void OpenGLRendererAPI::setViewport(int x, int y, int width, int height) {
        glViewport(x, y, width, height);
    }

    void OpenGLRendererAPI::setClearColor(float red, float green, float blue, float alpha) {
        glClearColor(red, green, blue, alpha);
    }

    void OpenGLRendererAPI::clear(bool clearColor, bool clearDepth) {
        GLbitfield mask = 0;
        if (clearColor) {
            mask |= GL_COLOR_BUFFER_BIT;
        }
        if (clearDepth) {
            mask |= GL_DEPTH_BUFFER_BIT;
        }
        if (mask != 0) {
            glClear(mask);
        }
    }

    void OpenGLRendererAPI::useShader(GLuint shaderID) {
        glUseProgram(shaderID);
    }

    void OpenGLRendererAPI::bindVertexArray(GLuint vaoID) {
        glBindVertexArray(vaoID);
    }

    void OpenGLRendererAPI::bindTexture(GLuint textureID, GLuint slot) {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, textureID);
    }

    void OpenGLRendererAPI::drawIndexed(GLuint indexBufferID, GLuint indexCount) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, nullptr);
    }

    void OpenGLRendererAPI::setUniformMat4(GLuint shaderID, const std::string& name, const glm::mat4& matrix) {
        glUseProgram(shaderID);
        GLint location = glGetUniformLocation(shaderID, name.c_str());
        if (location >= 0) {
            glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
        }
    }

    void OpenGLRendererAPI::setUniformVec4(GLuint shaderID, const std::string& name, const glm::vec4& vector) {
        glUseProgram(shaderID);
        GLint location = glGetUniformLocation(shaderID, name.c_str());
        if (location >= 0) {
            glUniform4fv(location, 1, glm::value_ptr(vector));
        }
    }

    void OpenGLRendererAPI::setUniformFloat(GLuint shaderID, const std::string& name, float value) {
        glUseProgram(shaderID);
        GLint location = glGetUniformLocation(shaderID, name.c_str());
        if (location >= 0) {
            glUniform1f(location, value);
        }
    }

    void OpenGLRendererAPI::setUniformInt(GLuint shaderID, const std::string& name, int value) {
        glUseProgram(shaderID);
        GLint location = glGetUniformLocation(shaderID, name.c_str());
        if (location >= 0) {
            glUniform1i(location, value);
        }
    }

    void OpenGLRendererAPI::setDepthTest(bool enabled) {
        if (enabled) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
    }

}