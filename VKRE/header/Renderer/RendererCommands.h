#pragma once

#include <ResourceManager/ResourceHandles.h>

#include <glm/glm.hpp>
#include <cstdint>

namespace VKRE {

    struct DrawPushConstants {
        glm::mat4 Transform = glm::mat4(1.0f);
        uint64_t VertexBufferAddress = 0;
        uint32_t Texture2DIndex = 0;
        uint32_t TextureCubeIndex = 0;
    };

    struct SkyboxPushConstants {
        glm::mat4 ViewProjection = glm::mat4(1.0f);
        uint32_t TextureCubeIndex = 0;
    };

    struct PostProcessConstants {
        float Exposure = 1.0f;
    };

    struct MeshDrawCommand {
        GPUBufferHandle IndexBuffer;
        uint32_t IndexCount;
        uint32_t BaseIndex;
        uint32_t BaseVertex;
        uint64_t VertexBufferAddress;
        glm::mat4 Transform;
        glm::mat4 ViewProjection;
        uint32_t TextureIndex = 0;
    };

    struct SkyboxDrawCommand {
        SkyboxPushConstants PushConstants;
    };

}
