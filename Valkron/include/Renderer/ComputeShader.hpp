#pragma once

#include "Core/Core.hpp"

#include <string>

namespace Valkron {

    class VALKRON_API ComputeShader {
        public:
            explicit ComputeShader(const std::string& computePath);
            ~ComputeShader();

            void bind() const;
            void unbind() const;
            void dispatch(unsigned int groupX, unsigned int groupY = 1, unsigned int groupZ = 1) const;

            void setInt(const std::string& name, int value) const;
            void setFloat(const std::string& name, float value) const;

        private:
            unsigned int m_programID = 0;

            static std::string readFile(const std::string& filePath);
            static unsigned int compileShader(unsigned int type, const std::string& source);
            static void checkProgramLink(unsigned int programID);
            int getUniformLocation(const std::string& name) const;
    };

}
