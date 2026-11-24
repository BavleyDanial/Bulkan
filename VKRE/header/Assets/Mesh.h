#pragma once

#include <ResourceManager/ResourceHandles.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace VKRE {

    struct Texture2DAsset;

    struct SubMesh {
        uint32_t BaseIndex;
        uint32_t IndexCount;
        int32_t BaseVertex;
        uint32_t VertexCount;
        int32_t TextureIndex = -1; // index into MeshAsset::Textures, -1 = no texture
    };

    struct MeshNode {
        glm::mat4 LocalTransform;
        int32_t ParentIndex = -1;
        uint32_t SubMeshOffset;
        uint32_t SubMeshCount;
    };

    struct MeshAsset {
        std::string Name;
        std::string Path;

        std::vector<SubMesh> SubMeshes;
        GPUBufferHandle VertexBuffer;
        GPUBufferHandle IndexBuffer;
        uint64_t VertexBufferAddress = 0;

        std::vector<MeshNode> Nodes;
        std::vector<uint32_t> NodeSubMeshIndices;
        std::vector<std::string> NodeNames;

        std::vector<ResourceHandle<Texture2DAsset>> Textures;
    };

}
