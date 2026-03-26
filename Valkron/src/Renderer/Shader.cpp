#include "Renderer/Shader.hpp"

#include "Core/FileSystem.hpp"
#include "Core/Log.hpp"
#include "glad/gl.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace Valkron {

    Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath) {
        const std::string vertexSource = readFile(vertexPath);
        const std::string fragmentSource = readFile(fragmentPath);

        unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

        m_programID = glCreateProgram();
        glAttachShader(m_programID, vertexShader);
        glAttachShader(m_programID, fragmentShader);
        glLinkProgram(m_programID);
        checkProgramLink(m_programID);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    Shader::~Shader() {
        glDeleteProgram(m_programID);
    }

    void Shader::bind() const {
        glUseProgram(m_programID);
    }

    void Shader::unbind() const {
        glUseProgram(0);
    }

    void Shader::setInt(const std::string& name, int value) const {
        glUniform1i(getUniformLocation(name), value);
    }

    void Shader::setFloat(const std::string& name, float value) const {
        glUniform1f(getUniformLocation(name), value);
    }

    void Shader::setVec2(const std::string& name, const float* value) const {
        glUniform2fv(getUniformLocation(name), 1, value);
    }

    void Shader::setVec3(const std::string& name, const float* value) const {
        glUniform3fv(getUniformLocation(name), 1, value);
    }

    void Shader::setVec4(const std::string& name, const float* value) const {
        glUniform4fv(getUniformLocation(name), 1, value);
    }

    void Shader::setMat3(const std::string& name, const float* value) const {
        glUniformMatrix3fv(getUniformLocation(name), 1, GL_FALSE, value);
    }

    void Shader::setMat4(const std::string& name, const float* value) const {
        glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, value);
    }

    std::string Shader::readFile(const std::string& filePath) {
        const std::filesystem::path resolvedPath = FileSystem::resolveExistingPath(filePath);
        std::ifstream file(resolvedPath, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open shader file: " + resolvedPath.string());
            return {};
        }

        std::ostringstream stream;
        stream << file.rdbuf();
        return stream.str();
    }

    unsigned int Shader::compileShader(unsigned int type, const std::string& source) {
        unsigned int shader = glCreateShader(type);
        const char* sourceCStr = source.c_str();
        glShaderSource(shader, 1, &sourceCStr, nullptr);
        glCompileShader(shader);

        int success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            int logLength = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<char> infoLog(static_cast<std::size_t>(logLength));
            glGetShaderInfoLog(shader, logLength, nullptr, infoLog.data());
            LOG_ERROR("Shader compilation failed: " + std::string(infoLog.begin(), infoLog.end()));
        }

        return shader;
    }

    void Shader::checkProgramLink(unsigned int programID) {
        int success = 0;
        glGetProgramiv(programID, GL_LINK_STATUS, &success);
        if (!success) {
            int logLength = 0;
            glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<char> infoLog(static_cast<std::size_t>(logLength));
            glGetProgramInfoLog(programID, logLength, nullptr, infoLog.data());
            LOG_ERROR("Shader program link failed: " + std::string(infoLog.begin(), infoLog.end()));
        }
    }

    int Shader::getUniformLocation(const std::string& name) const {
        return glGetUniformLocation(m_programID, name.c_str());
    }

}