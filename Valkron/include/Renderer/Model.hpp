#pragma once

#include "Core/Core.hpp"
#include "Renderer/Buffers.hpp"
#include "Renderer/Shader.hpp"
#include "Renderer/Texture.hpp"

#include "glm/glm.hpp"

#include <memory>
#include <string>
#include <vector>

namespace Valkron {

    struct VALKRON_API ModelMaterial {
        glm::vec3 diffuseColor{1.0f, 1.0f, 1.0f};
        glm::vec3 specularColor{1.0f, 1.0f, 1.0f};
        float shininess = 32.0f;
        std::shared_ptr<Texture> diffuseTexture = nullptr;
        std::shared_ptr<Texture> specularTexture = nullptr;
        std::shared_ptr<Texture> normalTexture = nullptr;
        std::vector<std::string> sourceTexturePaths;
    };

    class VALKRON_API Model {
        public:
            struct Mesh {
                std::unique_ptr<VertexArray> vertexArray;
                std::unique_ptr<VertexBuffer> vertexBuffer;
                std::unique_ptr<IndexBuffer> indexBuffer;
                ModelMaterial material;
                glm::vec3 localBoundsMin{0.0f, 0.0f, 0.0f};
                glm::vec3 localBoundsMax{0.0f, 0.0f, 0.0f};
                bool hasLocalBounds = false;

                Mesh() = default;
                ~Mesh() = default;
                Mesh(const Mesh&) = delete;
                Mesh& operator=(const Mesh&) = delete;
                Mesh(Mesh&&) noexcept = default;
                Mesh& operator=(Mesh&&) noexcept = default;
            };

            Model() = default;
            ~Model() = default;
            Model(const Model&) = delete;
            Model& operator=(const Model&) = delete;
            Model(Model&&) noexcept = default;
            Model& operator=(Model&&) noexcept = default;

            bool loadFromFile(const std::string& modelPath);
            void draw(const Shader& shader) const;
            bool getLocalBounds(glm::vec3& outMin, glm::vec3& outMax) const;

            bool isLoaded() const { return !m_meshes.empty(); }
            const std::string& getPath() const { return m_modelPath; }
            const std::vector<std::string>& getReferencedTexturePaths() const { return m_referencedTexturePaths; }

        private:
            std::string m_modelPath;
            std::vector<Mesh> m_meshes;
            std::vector<std::string> m_referencedTexturePaths;
            glm::vec3 m_localBoundsMin{0.0f, 0.0f, 0.0f};
            glm::vec3 m_localBoundsMax{0.0f, 0.0f, 0.0f};
            bool m_hasLocalBounds = false;
    };

}
