#include "Renderer/Model.hpp"

#include "Core/FileSystem.hpp"
#include "Core/Log.hpp"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "glad/gl.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Valkron {

    struct ModelVertex {
        glm::vec3 position{0.0f, 0.0f, 0.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        glm::vec2 texCoord{0.0f, 0.0f};
    };

    std::string toLowercaseCopy(const std::string& value) {
        std::string lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return lowered;
    }

    std::string trimCopy(const std::string& value) {
        const std::size_t begin = value.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) {
            return {};
        }

        const std::size_t end = value.find_last_not_of(" \t\r\n");
        return value.substr(begin, end - begin + 1);
    }

    std::string normalizePathKey(const std::filesystem::path& path) {
        return toLowercaseCopy(path.lexically_normal().generic_string());
    }

    std::filesystem::path resolveMaterialTexturePath(const aiString& texturePath, const std::filesystem::path& modelDirectory) {
        const std::filesystem::path rawTexturePath(texturePath.C_Str());
        if (rawTexturePath.empty()) {
            return {};
        }

        std::error_code errorCode;
        if (rawTexturePath.is_absolute() && std::filesystem::exists(rawTexturePath, errorCode)) {
            return rawTexturePath;
        }

        std::filesystem::path candidate = modelDirectory / rawTexturePath;
        if (std::filesystem::exists(candidate, errorCode)) {
            return candidate;
        }

        if (!rawTexturePath.filename().empty()) {
            candidate = modelDirectory / rawTexturePath.filename();
            if (std::filesystem::exists(candidate, errorCode)) {
                return candidate;
            }
        }

        const std::filesystem::path fallbackPath = FileSystem::resolveExistingPath(rawTexturePath);
        if (std::filesystem::exists(fallbackPath, errorCode)) {
            return fallbackPath;
        }

        return {};
    }

    std::shared_ptr<Texture> loadMaterialTexture(
        aiMaterial* material,
        aiTextureType textureType,
        const std::filesystem::path& modelDirectory,
        std::string& outResolvedPath
    ) {
        outResolvedPath.clear();
        if (material == nullptr || material->GetTextureCount(textureType) == 0) {
            return nullptr;
        }

        aiString texturePath;
        if (material->GetTexture(textureType, 0, &texturePath) != AI_SUCCESS) {
            return nullptr;
        }

        const std::filesystem::path resolvedPath = resolveMaterialTexturePath(texturePath, modelDirectory);
        if (resolvedPath.empty()) {
            LOG_WARN("Unable to resolve material texture path: " + std::string(texturePath.C_Str()));
            return nullptr;
        }

        auto texture = std::make_shared<Texture>();
        if (!texture->loadTexture(resolvedPath.string())) {
            LOG_WARN("Failed to load material texture: " + resolvedPath.string());
            return nullptr;
        }

        outResolvedPath = resolvedPath.string();
        return texture;
    }

    void verifyObjMaterialLibraries(const std::filesystem::path& modelPath) {
        if (toLowercaseCopy(modelPath.extension().string()) != ".obj") {
            return;
        }

        std::ifstream objFile(modelPath);
        if (!objFile.is_open()) {
            return;
        }

        std::unordered_set<std::string> seenLibraries;
        int resolvedCount = 0;
        int missingCount = 0;

        std::string line;
        while (std::getline(objFile, line)) {
            const std::string trimmedLine = trimCopy(line);
            if (trimmedLine.size() < 7 || trimmedLine.rfind("mtllib", 0) != 0) {
                continue;
            }

            std::istringstream lineStream(trimmedLine.substr(6));
            std::string mtlName;
            while (lineStream >> mtlName) {
                const std::filesystem::path mtlPath = modelPath.parent_path() / mtlName;
                const std::string key = normalizePathKey(mtlPath);
                if (!seenLibraries.insert(key).second) {
                    continue;
                }

                std::error_code errorCode;
                if (std::filesystem::exists(mtlPath, errorCode)) {
                    ++resolvedCount;
                } else {
                    ++missingCount;
                    LOG_WARN("OBJ references missing MTL file: " + mtlPath.string());
                }
            }
        }

        if (resolvedCount > 0) {
            LOG_INFO("Resolved " + std::to_string(resolvedCount) + " MTL file(s) for OBJ: " + modelPath.string());
        }
        if (resolvedCount == 0 && missingCount == 0) {
            LOG_WARN("OBJ file has no mtllib declaration: " + modelPath.string());
        }
    }

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

        std::string diffuseTexturePath;
        material.diffuseTexture = loadMaterialTexture(aiMat, aiTextureType_DIFFUSE, modelDirectory, diffuseTexturePath);
        if (!diffuseTexturePath.empty()) {
            material.sourceTexturePaths.push_back(diffuseTexturePath);
        }

        std::string specularTexturePath;
        material.specularTexture = loadMaterialTexture(aiMat, aiTextureType_SPECULAR, modelDirectory, specularTexturePath);
        if (!specularTexturePath.empty()) {
            material.sourceTexturePaths.push_back(specularTexturePath);
        }

        std::string normalTexturePath;
        material.normalTexture = loadMaterialTexture(aiMat, aiTextureType_NORMALS, modelDirectory, normalTexturePath);
        if (material.normalTexture == nullptr) {
            material.normalTexture = loadMaterialTexture(aiMat, aiTextureType_HEIGHT, modelDirectory, normalTexturePath);
        }
        if (!normalTexturePath.empty()) {
            material.sourceTexturePaths.push_back(normalTexturePath);
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

        verifyObjMaterialLibraries(resolvedModelPath);

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
        m_referencedTexturePaths.clear();
        if (m_meshes.empty()) {
            LOG_ERROR("Model contains no renderable meshes: " + m_modelPath);
            return false;
        }

        std::unordered_set<std::string> seenTexturePaths;
        for (const Mesh& mesh : m_meshes) {
            for (const std::string& texturePath : mesh.material.sourceTexturePaths) {
                const std::filesystem::path normalizedPath = std::filesystem::path(texturePath).lexically_normal();
                const std::string key = normalizePathKey(normalizedPath);
                if (!seenTexturePaths.insert(key).second) {
                    continue;
                }

                m_referencedTexturePaths.push_back(normalizedPath.string());
            }
        }

        LOG_INFO(
            "Loaded model with " + std::to_string(m_meshes.size()) + " mesh(es) and " +
            std::to_string(m_referencedTexturePaths.size()) + " referenced material texture(s): " + m_modelPath
        );
        return true;
    }

    void Model::draw(const Shader& shader) const {
        for (const Mesh& mesh : m_meshes) {
            if (!mesh.vertexArray || !mesh.indexBuffer) {
                continue;
            }

            const bool hasDiffuseMap = mesh.material.diffuseTexture != nullptr;
            const bool hasSpecularMap = mesh.material.specularTexture != nullptr;
            shader.setInt("u_Material.hasDiffuseMap", hasDiffuseMap ? 1 : 0);
            shader.setInt("u_Material.hasSpecularMap", hasSpecularMap ? 1 : 0);
            shader.setVec3("u_Material.diffuseColor", &mesh.material.diffuseColor.x);
            shader.setVec3("u_Material.specularColor", &mesh.material.specularColor.x);
            shader.setFloat("u_Material.shininess", mesh.material.shininess);

            if (hasDiffuseMap) {
                mesh.material.diffuseTexture->bind(0);
                shader.setInt("u_Material.diffuseMap", 0);
            }

            if (hasSpecularMap) {
                mesh.material.specularTexture->bind(1);
                shader.setInt("u_Material.specularMap", 1);
            }

            mesh.vertexArray->bind();
            mesh.indexBuffer->bind();
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indexBuffer->getCount()), GL_UNSIGNED_INT, nullptr);

            if (hasDiffuseMap) {
                mesh.material.diffuseTexture->unbind();
            }

            if (hasSpecularMap) {
                mesh.material.specularTexture->unbind();
            }
        }
    }

}
