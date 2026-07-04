#pragma once

#include "Core/Core.hpp"
#include <string>

namespace Valkron {

    class VALKRON_API Shader {
        public:
            Shader(const std::string& vertexPath, const std::string& fragmentPath);
            ~Shader();

            void bind() const;
            void unbind() const;

            void setInt(const std::string& name, int value) const;
            void setFloat(const std::string& name, float value) const;
            void setVec2(const std::string& name, const float* value) const;
            void setVec3(const std::string& name, const float* value) const;
            void setVec4(const std::string& name, const float* value) const;
            void setMat3(const std::string& name, const float* value) const;
            void setMat4(const std::string& name, const float* value) const;

            unsigned int getProgramID() const { return m_programID; }

        private:
            unsigned int m_programID = 0;

            static std::string readFile(const std::string& filePath);
            static unsigned int compileShader(unsigned int type, const std::string& source);
            static void checkProgramLink(unsigned int programID);
            int getUniformLocation(const std::string& name) const;
    };

}