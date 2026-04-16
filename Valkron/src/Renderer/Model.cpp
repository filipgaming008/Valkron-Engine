#include "Renderer/Model.hpp"

#include "Core/FileSystem.hpp"
#include "Core/Log.hpp"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "glad/gl.h"

#include "glm/gtc/type_ptr.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
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

    glm::mat4 convertAssimpMatrixToGlm(const aiMatrix4x4& matrix) {
        glm::mat4 converted(1.0f);
        converted[0][0] = matrix.a1;
        converted[1][0] = matrix.a2;
        converted[2][0] = matrix.a3;
        converted[3][0] = matrix.a4;
        converted[0][1] = matrix.b1;
        converted[1][1] = matrix.b2;
        converted[2][1] = matrix.b3;
        converted[3][1] = matrix.b4;
        converted[0][2] = matrix.c1;
        converted[1][2] = matrix.c2;
        converted[2][2] = matrix.c3;
        converted[3][2] = matrix.c4;
        converted[0][3] = matrix.d1;
        converted[1][3] = matrix.d2;
        converted[2][3] = matrix.d3;
        converted[3][3] = matrix.d4;
        return converted;
    }

    void transformBoundsByMatrix(
        const glm::vec3& boundsMin,
        const glm::vec3& boundsMax,
        const glm::mat4& transform,
        glm::vec3& outBoundsMin,
        glm::vec3& outBoundsMax
    ) {
        const std::array<glm::vec3, 8> corners = {
            glm::vec3(boundsMin.x, boundsMin.y, boundsMin.z),
            glm::vec3(boundsMin.x, boundsMin.y, boundsMax.z),
            glm::vec3(boundsMin.x, boundsMax.y, boundsMin.z),
            glm::vec3(boundsMin.x, boundsMax.y, boundsMax.z),
            glm::vec3(boundsMax.x, boundsMin.y, boundsMin.z),
            glm::vec3(boundsMax.x, boundsMin.y, boundsMax.z),
            glm::vec3(boundsMax.x, boundsMax.y, boundsMin.z),
            glm::vec3(boundsMax.x, boundsMax.y, boundsMax.z)
        };

        outBoundsMin = glm::vec3(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        );
        outBoundsMax = glm::vec3(
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()
        );

        for (const glm::vec3& corner : corners) {
            const glm::vec3 transformedCorner = glm::vec3(transform * glm::vec4(corner, 1.0f));
            outBoundsMin.x = std::min(outBoundsMin.x, transformedCorner.x);
            outBoundsMin.y = std::min(outBoundsMin.y, transformedCorner.y);
            outBoundsMin.z = std::min(outBoundsMin.z, transformedCorner.z);
            outBoundsMax.x = std::max(outBoundsMax.x, transformedCorner.x);
            outBoundsMax.y = std::max(outBoundsMax.y, transformedCorner.y);
            outBoundsMax.z = std::max(outBoundsMax.z, transformedCorner.z);
        }
    }

    struct UVTransform2D {
        glm::vec2 translation{0.0f, 0.0f};
        glm::vec2 scaling{1.0f, 1.0f};
        float rotationRadians = 0.0f;
        bool enabled = false;
    };

    UVTransform2D getUvTransformForMesh(const aiMesh* mesh, const aiScene* scene) {
        UVTransform2D transform;
        if (mesh == nullptr || scene == nullptr || !scene->HasMaterials()) {
            return transform;
        }

        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        if (material == nullptr) {
            return transform;
        }

        aiUVTransform assimpTransform;
        bool hasUvTransform = material->Get(AI_MATKEY_UVTRANSFORM(aiTextureType_BASE_COLOR, 0), assimpTransform) == AI_SUCCESS;
        if (!hasUvTransform) {
            hasUvTransform = material->Get(AI_MATKEY_UVTRANSFORM(aiTextureType_DIFFUSE, 0), assimpTransform) == AI_SUCCESS;
        }

        if (!hasUvTransform) {
            return transform;
        }

        transform.translation = glm::vec2(assimpTransform.mTranslation.x, assimpTransform.mTranslation.y);
        transform.scaling = glm::vec2(assimpTransform.mScaling.x, assimpTransform.mScaling.y);
        transform.rotationRadians = assimpTransform.mRotation;
        transform.enabled = true;
        return transform;
    }

    glm::vec2 applyUvTransform(const glm::vec2& uv, const UVTransform2D& transform) {
        if (!transform.enabled) {
            return uv;
        }

        glm::vec2 transformedUv = uv * transform.scaling;
        const float cosine = std::cos(transform.rotationRadians);
        const float sine = std::sin(transform.rotationRadians);
        transformedUv = glm::vec2(
            transformedUv.x * cosine - transformedUv.y * sine,
            transformedUv.x * sine + transformedUv.y * cosine
        );

        return transformedUv + transform.translation;
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
        if (!texture->loadTexture(resolvedPath.string(), false)) {
            LOG_WARN("Failed to load material texture: " + resolvedPath.string());
            return nullptr;
        }

        outResolvedPath = resolvedPath.string();
        return texture;
    }

    void collectMaterialTexturePaths(
        aiMaterial* material,
        aiTextureType textureType,
        const std::filesystem::path& modelDirectory,
        std::vector<std::string>& outResolvedPaths
    ) {
        if (material == nullptr) {
            return;
        }

        const unsigned int textureCount = material->GetTextureCount(textureType);
        if (textureCount == 0) {
            return;
        }

        for (unsigned int textureIndex = 0; textureIndex < textureCount; ++textureIndex) {
            aiString texturePath;
            if (material->GetTexture(textureType, textureIndex, &texturePath) != AI_SUCCESS) {
                continue;
            }

            const std::filesystem::path resolvedPath = resolveMaterialTexturePath(texturePath, modelDirectory);
            if (resolvedPath.empty()) {
                continue;
            }

            outResolvedPaths.push_back(resolvedPath.string());
        }
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

        float opacity = material.opacity;
        if (aiMat->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
            material.opacity = std::clamp(opacity, 0.0f, 1.0f);
        }

        std::string diffuseTexturePath;
        material.diffuseTexture = loadMaterialTexture(aiMat, aiTextureType_DIFFUSE, modelDirectory, diffuseTexturePath);
        material.hasAlphaTexture = material.diffuseTexture != nullptr && material.diffuseTexture->getChannels() == 4;
        std::string specularTexturePath;
        material.specularTexture = loadMaterialTexture(aiMat, aiTextureType_SPECULAR, modelDirectory, specularTexturePath);
        std::string normalTexturePath;
        material.normalTexture = loadMaterialTexture(aiMat, aiTextureType_NORMALS, modelDirectory, normalTexturePath);
        if (material.normalTexture == nullptr) {
            material.normalTexture = loadMaterialTexture(aiMat, aiTextureType_HEIGHT, modelDirectory, normalTexturePath);
        }

        std::unordered_set<std::string> seenTexturePathKeys;
        auto appendTexturePath = [&](const std::string& resolvedPath) {
            if (resolvedPath.empty()) {
                return;
            }

            const std::string normalizedKey = normalizePathKey(std::filesystem::path(resolvedPath));
            if (!seenTexturePathKeys.insert(normalizedKey).second) {
                return;
            }

            material.sourceTexturePaths.push_back(resolvedPath);
        };

        appendTexturePath(diffuseTexturePath);
        appendTexturePath(specularTexturePath);
        appendTexturePath(normalTexturePath);

        // Collect additional material maps so FBX/glTF and other PBR-oriented formats
        // have their external textures discovered and registered for runtime linking.
        const std::array<aiTextureType, 12> textureTypesToCollect = {
            aiTextureType_DIFFUSE,
            aiTextureType_SPECULAR,
            aiTextureType_NORMALS,
            aiTextureType_HEIGHT,
            aiTextureType_BASE_COLOR,
            aiTextureType_METALNESS,
            aiTextureType_DIFFUSE_ROUGHNESS,
            aiTextureType_AMBIENT_OCCLUSION,
            aiTextureType_LIGHTMAP,
            aiTextureType_EMISSIVE,
            aiTextureType_OPACITY,
            aiTextureType_UNKNOWN
        };

        std::vector<std::string> collectedTexturePaths;
        for (const aiTextureType textureType : textureTypesToCollect) {
            collectMaterialTexturePaths(aiMat, textureType, modelDirectory, collectedTexturePaths);
        }

        for (const std::string& resolvedPath : collectedTexturePaths) {
            appendTexturePath(resolvedPath);
        }

        return material;
    }

    Model::Mesh buildMesh(
        const aiMesh* mesh,
        const aiScene* scene,
        const std::filesystem::path& modelDirectory,
        const glm::mat4& nodeTransform,
        int sourceMeshIndex
    ) {
        Model::Mesh builtMesh;
        if (mesh == nullptr) {
            return builtMesh;
        }

        builtMesh.sourceMeshIndex = sourceMeshIndex;
        builtMesh.nodeTransform = nodeTransform;
        const UVTransform2D uvTransform = getUvTransformForMesh(mesh, scene);

        std::vector<ModelVertex> vertices;
        vertices.reserve(mesh->mNumVertices);

        glm::vec3 meshBoundsMin(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        );
        glm::vec3 meshBoundsMax(
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()
        );

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            ModelVertex vertex;
            vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

            meshBoundsMin.x = std::min(meshBoundsMin.x, vertex.position.x);
            meshBoundsMin.y = std::min(meshBoundsMin.y, vertex.position.y);
            meshBoundsMin.z = std::min(meshBoundsMin.z, vertex.position.z);
            meshBoundsMax.x = std::max(meshBoundsMax.x, vertex.position.x);
            meshBoundsMax.y = std::max(meshBoundsMax.y, vertex.position.y);
            meshBoundsMax.z = std::max(meshBoundsMax.z, vertex.position.z);

            if (mesh->HasNormals()) {
                vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            }

            if (mesh->HasTextureCoords(0)) {
                const glm::vec2 baseUv(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
                vertex.texCoord = applyUvTransform(baseUv, uvTransform);
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
        builtMesh.rawLocalBoundsMin = meshBoundsMin;
        builtMesh.rawLocalBoundsMax = meshBoundsMax;
        builtMesh.hasRawLocalBounds = true;
        transformBoundsByMatrix(meshBoundsMin, meshBoundsMax, nodeTransform, builtMesh.localBoundsMin, builtMesh.localBoundsMax);
        builtMesh.hasLocalBounds = true;

        return builtMesh;
    }

    void processNode(
        aiNode* node,
        const aiScene* scene,
        const std::filesystem::path& modelDirectory,
        const glm::mat4& parentTransform,
        std::vector<Model::Mesh>& outMeshes
    ) {
        if (node == nullptr || scene == nullptr) {
            return;
        }

        const glm::mat4 nodeTransform = parentTransform * convertAssimpMatrixToGlm(node->mTransformation);

        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            const unsigned int meshIndex = node->mMeshes[i];
            const aiMesh* mesh = scene->mMeshes[meshIndex];
            Model::Mesh builtMesh = buildMesh(
                mesh,
                scene,
                modelDirectory,
                nodeTransform,
                static_cast<int>(meshIndex)
            );
            if (builtMesh.vertexArray != nullptr && builtMesh.vertexBuffer != nullptr && builtMesh.indexBuffer != nullptr) {
                outMeshes.push_back(std::move(builtMesh));
            }
        }

        for (unsigned int i = 0; i < node->mNumChildren; ++i) {
            processNode(node->mChildren[i], scene, modelDirectory, nodeTransform, outMeshes);
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
            m_hasLocalBounds = false;
            m_hasTransparentMaterials = false;
            return false;
        }

        std::vector<Mesh> loadedMeshes;
        const std::filesystem::path modelDirectory = resolvedModelPath.has_parent_path() ? resolvedModelPath.parent_path() : std::filesystem::path{};
        processNode(scene->mRootNode, scene, modelDirectory, glm::mat4(1.0f), loadedMeshes);

        m_meshes = std::move(loadedMeshes);
        m_referencedTexturePaths.clear();
        m_hasTransparentMaterials = false;
        if (m_meshes.empty()) {
            LOG_ERROR("Model contains no renderable meshes: " + m_modelPath);
            m_hasLocalBounds = false;
            return false;
        }

        bool hasAnyBounds = false;
        glm::vec3 modelBoundsMin(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        );
        glm::vec3 modelBoundsMax(
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()
        );

        for (const Mesh& mesh : m_meshes) {
            if (mesh.material.opacity < 0.999f || mesh.material.hasAlphaTexture) {
                m_hasTransparentMaterials = true;
            }

            if (!mesh.hasLocalBounds) {
                continue;
            }

            hasAnyBounds = true;
            modelBoundsMin.x = std::min(modelBoundsMin.x, mesh.localBoundsMin.x);
            modelBoundsMin.y = std::min(modelBoundsMin.y, mesh.localBoundsMin.y);
            modelBoundsMin.z = std::min(modelBoundsMin.z, mesh.localBoundsMin.z);
            modelBoundsMax.x = std::max(modelBoundsMax.x, mesh.localBoundsMax.x);
            modelBoundsMax.y = std::max(modelBoundsMax.y, mesh.localBoundsMax.y);
            modelBoundsMax.z = std::max(modelBoundsMax.z, mesh.localBoundsMax.z);
        }

        if (hasAnyBounds) {
            m_localBoundsMin = modelBoundsMin;
            m_localBoundsMax = modelBoundsMax;
            m_hasLocalBounds = true;
        } else {
            m_localBoundsMin = glm::vec3(0.0f, 0.0f, 0.0f);
            m_localBoundsMax = glm::vec3(0.0f, 0.0f, 0.0f);
            m_hasLocalBounds = false;
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

    void Model::draw(
        const Shader& shader,
        const glm::mat4& baseTransform,
        const std::vector<int>& meshIndices,
        bool applyNodeTransforms
    ) const {
        std::unordered_set<int> meshIndexFilter;
        if (!meshIndices.empty()) {
            meshIndexFilter.reserve(meshIndices.size());
            for (const int meshIndex : meshIndices) {
                if (meshIndex >= 0) {
                    meshIndexFilter.insert(meshIndex);
                }
            }
        }

        for (const Mesh& mesh : m_meshes) {
            if (!mesh.vertexArray || !mesh.indexBuffer) {
                continue;
            }

            if (!meshIndexFilter.empty() && meshIndexFilter.find(mesh.sourceMeshIndex) == meshIndexFilter.end()) {
                continue;
            }

            const glm::mat4 modelMatrix = applyNodeTransforms ? (baseTransform * mesh.nodeTransform) : baseTransform;
            shader.setMat4("u_Model", glm::value_ptr(modelMatrix));

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

    bool Model::getLocalBounds(glm::vec3& outMin, glm::vec3& outMax) const {
        static const std::vector<int> emptyMeshFilter;
        return getLocalBounds(outMin, outMax, emptyMeshFilter, true);
    }

    bool Model::getLocalBounds(
        glm::vec3& outMin,
        glm::vec3& outMax,
        const std::vector<int>& meshIndices,
        bool applyNodeTransforms
    ) const {
        if (m_meshes.empty()) {
            return false;
        }

        std::unordered_set<int> meshIndexFilter;
        if (!meshIndices.empty()) {
            meshIndexFilter.reserve(meshIndices.size());
            for (const int meshIndex : meshIndices) {
                if (meshIndex >= 0) {
                    meshIndexFilter.insert(meshIndex);
                }
            }
        }

        bool hasAnyBounds = false;
        glm::vec3 boundsMin(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        );
        glm::vec3 boundsMax(
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()
        );

        for (const Mesh& mesh : m_meshes) {
            if (!meshIndexFilter.empty() && meshIndexFilter.find(mesh.sourceMeshIndex) == meshIndexFilter.end()) {
                continue;
            }

            const bool hasMeshBounds = applyNodeTransforms ? mesh.hasLocalBounds : mesh.hasRawLocalBounds;
            if (!hasMeshBounds) {
                continue;
            }

            const glm::vec3& meshBoundsMin = applyNodeTransforms ? mesh.localBoundsMin : mesh.rawLocalBoundsMin;
            const glm::vec3& meshBoundsMax = applyNodeTransforms ? mesh.localBoundsMax : mesh.rawLocalBoundsMax;

            hasAnyBounds = true;
            boundsMin.x = std::min(boundsMin.x, meshBoundsMin.x);
            boundsMin.y = std::min(boundsMin.y, meshBoundsMin.y);
            boundsMin.z = std::min(boundsMin.z, meshBoundsMin.z);
            boundsMax.x = std::max(boundsMax.x, meshBoundsMax.x);
            boundsMax.y = std::max(boundsMax.y, meshBoundsMax.y);
            boundsMax.z = std::max(boundsMax.z, meshBoundsMax.z);
        }

        if (!hasAnyBounds) {
            return false;
        }

        outMin = boundsMin;
        outMax = boundsMax;
        return true;
    }

}
