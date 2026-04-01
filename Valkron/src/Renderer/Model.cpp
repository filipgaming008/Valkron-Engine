#include "Renderer/Model.hpp"

#include "Core/FileSystem.hpp"
#include "Core/Log.hpp"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "glad/gl.h"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace Valkron {

    struct ModelVertex {
        glm::vec3 position{0.0f, 0.0f, 0.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        glm::vec2 texCoord{0.0f, 0.0f};
    };

    ModelMaterial buildMaterial(const aiMesh* mesh, const aiScene* scene, const std::filesystem::path& modelDirectory) {
        ModelMaterial material;

        if (mesh == nullptr || scene == nullptr || !scene->HasMaterials()) {
            return material;
        }

        aiMaterial* aiMat = scene->mMaterials[mesh->mMaterialIndex];
        if (aiMat == nullptr) {
            return material;
        }

        aiColor3D diffuseColor(1.0f, 1.0f, 1.0f);
        if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor) == AI_SUCCESS) {
            material.diffuseColor = glm::vec3(diffuseColor.r, diffuseColor.g, diffuseColor.b);
        }

        aiColor3D specularColor(1.0f, 1.0f, 1.0f);
        if (aiMat->Get(AI_MATKEY_COLOR_SPECULAR, specularColor) == AI_SUCCESS) {
            material.specularColor = glm::vec3(specularColor.r, specularColor.g, specularColor.b);
        }

        float shininess = material.shininess;
        if (aiMat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
            material.shininess = shininess;
        }

        aiString texturePath;
        if (aiMat->GetTextureCount(aiTextureType_DIFFUSE) > 0 && aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
            std::filesystem::path diffuseTexturePath = modelDirectory / texturePath.C_Str();
            auto diffuseTexture = std::make_shared<Texture>();
            if (diffuseTexture->loadTexture(diffuseTexturePath.string())) {
                material.diffuseTexture = std::move(diffuseTexture);
            }
        }

        return material;
    }

    Model::Mesh buildMesh(const aiMesh* mesh, const aiScene* scene, const std::filesystem::path& modelDirectory) {
        Model::Mesh builtMesh;
        if (mesh == nullptr) {
            return builtMesh;
        }

        std::vector<ModelVertex> vertices;
        vertices.reserve(mesh->mNumVertices);

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            ModelVertex vertex;
            vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

            if (mesh->HasNormals()) {
                vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            }

            if (mesh->HasTextureCoords(0)) {
                vertex.texCoord = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            }

            vertices.push_back(vertex);
        }

        std::vector<unsigned int> indices;
        indices.reserve(static_cast<std::size_t>(mesh->mNumFaces) * 3u);

        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            const aiFace& face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                indices.push_back(face.mIndices[j]);
            }
        }

        if (vertices.empty() || indices.empty()) {
            return builtMesh;
        }

        builtMesh.vertexArray = std::make_unique<VertexArray>();
        builtMesh.vertexBuffer = std::make_unique<VertexBuffer>(vertices.data(), static_cast<unsigned int>(vertices.size() * sizeof(ModelVertex)));
        builtMesh.indexBuffer = std::make_unique<IndexBuffer>(indices.data(), static_cast<unsigned int>(indices.size()));

        VertexLayout layout;
        layout.push<float>(3);
        layout.push<float>(3);
        layout.push<float>(2);
        builtMesh.vertexArray->addBuffer(*builtMesh.vertexBuffer, layout);
        builtMesh.material = buildMaterial(mesh, scene, modelDirectory);

        return builtMesh;
    }

    void processNode(aiNode* node, const aiScene* scene, const std::filesystem::path& modelDirectory, std::vector<Model::Mesh>& outMeshes) {
        if (node == nullptr || scene == nullptr) {
            return;
        }

        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            Model::Mesh builtMesh = buildMesh(mesh, scene, modelDirectory);
            if (builtMesh.vertexArray != nullptr && builtMesh.vertexBuffer != nullptr && builtMesh.indexBuffer != nullptr) {
                outMeshes.push_back(std::move(builtMesh));
            }
        }

        for (unsigned int i = 0; i < node->mNumChildren; ++i) {
            processNode(node->mChildren[i], scene, modelDirectory, outMeshes);
        }
    }

    bool Model::loadFromFile(const std::string& modelPath) {
        const std::filesystem::path resolvedModelPath = FileSystem::resolveExistingPath(modelPath);
        m_modelPath = resolvedModelPath.string();

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            m_modelPath,
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_FlipUVs |
            aiProcess_JoinIdenticalVertices |
            aiProcess_CalcTangentSpace
        );

        if (scene == nullptr || scene->mRootNode == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0) {
            LOG_ERROR("Assimp failed to load model: " + m_modelPath + " | " + importer.GetErrorString());
            m_meshes.clear();
            return false;
        }

        std::vector<Mesh> loadedMeshes;
        const std::filesystem::path modelDirectory = resolvedModelPath.has_parent_path() ? resolvedModelPath.parent_path() : std::filesystem::path{};
        processNode(scene->mRootNode, scene, modelDirectory, loadedMeshes);

        m_meshes = std::move(loadedMeshes);
        if (m_meshes.empty()) {
            LOG_ERROR("Model contains no renderable meshes: " + m_modelPath);
            return false;
        }

        LOG_INFO("Loaded model with " + std::to_string(m_meshes.size()) + " mesh(es): " + m_modelPath);
        return true;
    }

    void Model::draw(const Shader& shader) const {
        for (const Mesh& mesh : m_meshes) {
            if (!mesh.vertexArray || !mesh.indexBuffer) {
                continue;
            }

            const bool hasDiffuseMap = mesh.material.diffuseTexture != nullptr;
            shader.setInt("u_Material.hasDiffuseMap", hasDiffuseMap ? 1 : 0);
            shader.setVec3("u_Material.diffuseColor", &mesh.material.diffuseColor.x);
            shader.setVec3("u_Material.specularColor", &mesh.material.specularColor.x);
            shader.setFloat("u_Material.shininess", mesh.material.shininess);

            if (hasDiffuseMap) {
                mesh.material.diffuseTexture->bind(0);
                shader.setInt("u_Material.diffuseMap", 0);
            }

            mesh.vertexArray->bind();
            mesh.indexBuffer->bind();
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indexBuffer->getCount()), GL_UNSIGNED_INT, nullptr);

            if (hasDiffuseMap) {
                mesh.material.diffuseTexture->unbind();
            }
        }
    }

}
