#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

// TODO: Split this file into a few files for organisation

namespace VKRE {

    struct SceneUBO {
        glm::mat4 View = glm::mat4(1.0f);
        glm::mat4 Projection = glm::mat4(1.0f);
        glm::mat4 ViewPorjection = glm::mat4(1.0f);
        glm::vec4 AmbientColor = glm::vec4(1.0f);
        glm::vec4 CameraPosition = glm::vec4(1.0f);
        glm::vec4 LightDirection = glm::vec4(1.0f);
        glm::vec4 LightColor = glm::vec4(1.0f);     // w is intensity
    };

    enum class ShaderStage : uint8_t {
        None = 0,
        Vertex,
        Fragment,
        Compute,
    };

    enum class GPUBufferUsage : uint16_t {
        None        = 0,
        Vertex      = 1 << 0,
        Index       = 1 << 1,
        Uniform     = 1 << 2,
        Storage     = 1 << 3,
        Indirect    = 1 << 4,
        TransferSrc = 1 << 5,
        TransferDst = 1 << 6,
    };

    struct GPUBufferDesc {
        std::string DebugName = "";
        uint64_t Size = 0;
        GPUBufferUsage Usage = GPUBufferUsage::None;
        bool HostVisible = false;
    };

    struct Vertex {
        glm::vec3 Position;
        float UVx;
        glm::vec3 Normal;
        float UVy;
        glm::vec4 Color;
    };

    enum class TextureUsage : uint16_t {
        None                    = 0,
        Sampled                 = 1 << 0,
        ColorAttachment         = 1 << 1,
        DepthStencilAttachment  = 1 << 2,
        InputAttachment         = 1 << 3,
        Storage                 = 1 << 4,
        TransferSrc             = 1 << 5,
        TransferDst             = 1 << 6,
    };

    // TODO: Add the rest of the formats
    enum class TextureFormat : uint16_t {
        None = 0,

        R8G8B8A8_SRGB,
        R8G8B8A8_UNORM,
        B8G8R8A8_SRGB,
        B8G8R8A8_UNORM,

        R16G16B16A16_SFLOAT,
        R32G32B32A32_SFLOAT,

        R16G16_SFLOAT,
        R8G8_UNORM,
        R32_UINT,

        D32_SFLOAT,
        D24_UNORM_S8_UINT,

        BC1_RGB_SRGB,
        BC1_RGB_UNORM,
        BC3_RGBA_SRGB,
        BC5_UNORM,
        BC7_RGBA_SRGB,
        BC7_RGBA_UNORM,
    };

    struct TextureDesc {
        std::string DebugName = "";
        uint32_t Width = 0;
        uint32_t Height = 0;
        TextureUsage Usage = TextureUsage::None;
        TextureFormat Format = TextureFormat::None;
        uint32_t MipLevels = 1; // > 1: the upload generates the chain from mip 0
    };

}

inline VKRE::GPUBufferUsage operator|(VKRE::GPUBufferUsage a, VKRE::GPUBufferUsage b) {
    return static_cast<VKRE::GPUBufferUsage>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

inline VKRE::GPUBufferUsage operator&(VKRE::GPUBufferUsage a, VKRE::GPUBufferUsage b) {
    return static_cast<VKRE::GPUBufferUsage>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}

inline VKRE::TextureUsage operator|(VKRE::TextureUsage a, VKRE::TextureUsage b) {
    return static_cast<VKRE::TextureUsage>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

inline VKRE::TextureUsage operator&(VKRE::TextureUsage a, VKRE::TextureUsage b) {
    return static_cast<VKRE::TextureUsage>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}
