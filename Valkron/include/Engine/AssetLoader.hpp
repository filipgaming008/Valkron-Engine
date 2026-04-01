#pragma once

#include "Core/Core.hpp"

#include <memory>
#include <string>
#include <vector>

namespace Valkron {

    class Texture;
    class Shader;
    class ComputeShader;
    class Model;

    class VALKRON_API AssetLoader {
        public:
            static void initialize();

            static bool loadTexture2D(const std::string& name, const std::string& texturePath);
            static bool loadTexture3D(const std::string& name, const std::string& texturePath, int depth = 1);
            static bool loadShader(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath);
            static bool loadComputeShader(const std::string& name, const std::string& computePath);
            static bool loadModel(const std::string& name, const std::string& modelPath);

            static std::shared_ptr<Texture> getTexture2D(const std::string& name);
            static std::shared_ptr<Texture> getTexture3D(const std::string& name);
            static std::shared_ptr<Shader> getShader(const std::string& name);
            static std::shared_ptr<ComputeShader> getComputeShader(const std::string& name);
            static std::shared_ptr<Model> getModel(const std::string& name);

            static std::vector<std::string> getTexture2DNames();
            static std::vector<std::string> getTexture3DNames();
            static std::vector<std::string> getShaderNames();
            static std::vector<std::string> getComputeShaderNames();
            static std::vector<std::string> getModelNames();

            static bool setModelShader(const std::string& modelName, const std::string& shaderName);
            static std::string getModelShader(const std::string& modelName);

            static void setActiveModel(const std::string& modelName);
            static std::string getActiveModel();

        private:
            static void loadDefaultAssets();
    };

}
