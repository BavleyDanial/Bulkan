#include <AssetsManagers/MeshImporter.h>

#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <glm/gtc/type_ptr.hpp>
#include <print>

namespace VKRE {

    std::optional<MeshImportResults> MeshImporter::LoadFromFile(const std::filesystem::path& path) {
        if (!std::filesystem::exists(path)) {
            std::println("MeshImporter::LoadFromFile couldn't find path to Mesh (path={})", path.string());
            return std::nullopt;
        }

        std::string pathStr = path.string();
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(pathStr,
            aiProcess_Triangulate           |
            aiProcess_ConvertToLeftHanded   |
            aiProcess_JoinIdenticalVertices |
            aiProcess_CalcTangentSpace      |
            aiProcess_GenSmoothNormals      |
            aiProcess_ImproveCacheLocality  |
            aiProcess_OptimizeMeshes
        );

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::println("MeshImporter::LoadFromFile couldn't import Mesh (path={})", path.string());
            return std::nullopt;
        }

        MeshImportResults results{};
        results.Name = path.stem().string();
        results.SubMeshes.reserve(scene->mNumMeshes);

        uint32_t globalVertexOffset = 0;
        int32_t globalIndexOffset = 0;

        for (uint32_t m = 0; m < scene->mNumMeshes; m++) {
            aiMesh* mesh = scene->mMeshes[m];

            SubMesh submesh{};
            submesh.IndexCount = mesh->mNumFaces * 3;
            submesh.VertexCount = mesh->mNumVertices;
            submesh.BaseIndex = globalIndexOffset;
            submesh.BaseVertex = globalVertexOffset;
            submesh.TextureIndex = -1;

            if (mesh->mMaterialIndex >= 0) {
                aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
                aiString texturePath;

                if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS || mat->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath) == AI_SUCCESS) {
                    std::string pathStr = texturePath.C_Str();
                    auto it = std::find(results.TexturePaths.begin(), results.TexturePaths.end(), pathStr);
                    if (it != results.TexturePaths.end()) {
                        submesh.TextureIndex = static_cast<int32_t>(std::distance(results.TexturePaths.begin(), it));
                    } else {
                        submesh.TextureIndex = static_cast<int32_t>(results.TexturePaths.size());
                        results.TexturePaths.push_back(pathStr);
                    }
                }
            }

            for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
                Vertex v{};
                v.Position = ToGLM(mesh->mVertices[i]);

                if (mesh->HasTextureCoords(0)) {
                    v.UVx = mesh->mTextureCoords[0][i].x;
                    v.UVy = mesh->mTextureCoords[0][i].y;
                }

                if (mesh->HasNormals())
                    v.Normal = ToGLM(mesh->mNormals[i]);

                if (mesh->HasVertexColors(0))
                    v.Color = ToGLM(mesh->mColors[0][i]);
                else
                    v.Color = glm::vec4(1.0f);

                results.Vertices.push_back(v);
            }

            for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
                aiFace& face = mesh->mFaces[i];
                for (uint32_t j = 0; j < face.mNumIndices; j++) {
                    results.Indices.push_back(face.mIndices[j]);
                }
            }

            results.SubMeshes.push_back(submesh);
            globalVertexOffset += mesh->mNumVertices;
            globalIndexOffset += submesh.IndexCount;
        }

        results = FlattenNodeHierarchy(scene->mRootNode, -1, results);
        return results;
    }

    MeshImportResults& MeshImporter::FlattenNodeHierarchy(const aiNode* node, int32_t ParentIndex, MeshImportResults& results) {
        struct StackElement {
            const aiNode* AssimpNode;
            int32_t ParentIndex;
        };

        std::vector<StackElement> stack;
        stack.reserve(32);
        stack.push_back({ node, -1 });

        while (!stack.empty()) {
            StackElement current = stack.back();
            stack.pop_back();

            int32_t currentNodeIndex = static_cast<int32_t>(results.Nodes.size());

            MeshNode meshNode{};
            meshNode.LocalTransform = ToGLM(current.AssimpNode->mTransformation);
            meshNode.ParentIndex = current.ParentIndex;
            meshNode.SubMeshOffset = static_cast<uint32_t>(results.NodeSubMeshIndices.size());
            meshNode.SubMeshCount = current.AssimpNode->mNumMeshes;

            for (uint32_t i = 0; i < current.AssimpNode->mNumMeshes; i++)
                results.NodeSubMeshIndices.push_back(current.AssimpNode->mMeshes[i]);

            results.Nodes.push_back(meshNode);
            results.NodeNames.push_back(current.AssimpNode->mName.C_Str());

            for (int32_t i = static_cast<int32_t>(current.AssimpNode->mNumChildren) - 1; i >= 0; i--)
                stack.push_back({ current.AssimpNode->mChildren[i], currentNodeIndex });
        }

        return results;
    }

    glm::mat4 MeshImporter::ToGLM(const aiMatrix4x4& mat) {
        return glm::transpose(glm::make_mat4(&mat.a1));
    }

    glm::mat3 MeshImporter::ToGLM(const aiMatrix3x3& mat) {
        return glm::transpose(glm::make_mat3(&mat.a1));
    }

    glm::vec4 MeshImporter::ToGLM(const aiColor4D& vec) {
        return glm::make_vec4(&vec.a);
    }

    glm::vec3 MeshImporter::ToGLM(const aiVector3D& vec) {
        return glm::make_vec3(&vec.x);
    }

    glm::vec2 MeshImporter::ToGLM(const aiVector2D& vec) {
        return glm::make_vec2(&vec.x);
    }

}
