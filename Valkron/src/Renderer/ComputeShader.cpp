#include "Renderer/ComputeShader.hpp"

#include "Core/FileSystem.hpp"
#include "Core/Log.hpp"
#include "glad/gl.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace Valkron {

    ComputeShader::ComputeShader(const std::string& computePath) {
        const std::string computeSource = readFile(computePath);
        VALKRON_CORE_ASSERT(!computeSource.empty(), "Compute shader source is empty");

        unsigned int computeShader = compileShader(GL_COMPUTE_SHADER, computeSource);

        m_programID = glCreateProgram();
        VALKRON_CORE_ASSERT(m_programID != 0, "Failed to create compute shader program");

        glAttachShader(m_programID, computeShader);
        glLinkProgram(m_programID);
        checkProgramLink(m_programID);

        glDeleteShader(computeShader);
    }

    ComputeShader::~ComputeShader() {
        if (m_programID != 0) {
            glDeleteProgram(m_programID);
            m_programID = 0;
        }
    }

    void ComputeShader::bind() const {
        VALKRON_CORE_ASSERT(m_programID != 0, "Attempted to bind invalid compute shader program");
        glUseProgram(m_programID);
    }

    void ComputeShader::unbind() const {
        glUseProgram(0);
    }

    void ComputeShader::dispatch(unsigned int groupX, unsigned int groupY, unsigned int groupZ) const {
        VALKRON_CORE_ASSERT(m_programID != 0, "Attempted to dispatch with invalid compute shader program");
        bind();
        glDispatchCompute(groupX, groupY, groupZ);
    }

    void ComputeShader::setInt(const std::string& name, int value) const {
        glUniform1i(getUniformLocation(name), value);
    }

    void ComputeShader::setFloat(const std::string& name, float value) const {
        glUniform1f(getUniformLocation(name), value);
    }

    std::string ComputeShader::readFile(const std::string& filePath) {
        const std::filesystem::path resolvedPath = FileSystem::resolveExistingPath(filePath);
        std::ifstream file(resolvedPath, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open compute shader file: " + resolvedPath.string());
            return {};
        }

        std::ostringstream stream;
        stream << file.rdbuf();
        return stream.str();
    }

    unsigned int ComputeShader::compileShader(unsigned int type, const std::string& source) {
        VALKRON_CORE_ASSERT(!source.empty(), "Cannot compile compute shader from empty source");

        unsigned int shader = glCreateShader(type);
        VALKRON_CORE_ASSERT(shader != 0, "Failed to create compute shader object");

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
            LOG_ERROR("Compute shader compilation failed: " + std::string(infoLog.begin(), infoLog.end()));
            glDeleteShader(shader);
            VALKRON_CORE_ASSERT(false, "Compute shader compilation failed");
        }

        return shader;
    }

    void ComputeShader::checkProgramLink(unsigned int programID) {
        VALKRON_CORE_ASSERT(programID != 0, "Invalid compute shader program ID");

        int success = 0;
        glGetProgramiv(programID, GL_LINK_STATUS, &success);
        if (!success) {
            int logLength = 0;
            glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<char> infoLog(static_cast<std::size_t>(logLength));
            glGetProgramInfoLog(programID, logLength, nullptr, infoLog.data());
            LOG_ERROR("Compute shader program link failed: " + std::string(infoLog.begin(), infoLog.end()));
            VALKRON_CORE_ASSERT(false, "Compute shader program link failed");
        }
    }

    int ComputeShader::getUniformLocation(const std::string& name) const {
        return glGetUniformLocation(m_programID, name.c_str());
    }

}
