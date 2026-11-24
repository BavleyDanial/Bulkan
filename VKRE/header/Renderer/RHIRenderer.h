#pragma once

#include <ResourceManager/ResourceHandles.h>
#include <ResourceManager/Resources.h>

#include <array>
#include <span>
#include <cstddef>

namespace VKRE {

    using Bytes = std::span<const std::byte>;
    using CubeFaces = std::array<Bytes, 6>; // +X, -X, +Y, -Y, +Z, -Z

    struct RHIRendererSettings {
        uint32_t MaxBuffers = 4096;
        uint32_t MaxTextures2D = 8192;
        uint32_t MaxTexturesCube = 512;
        // TODO: Uploads block today. Add a staging budget here when they get batched into the frame.
    };

    using RHI_API = class VulkanRHI; // TODO: Change VulkanResourceCache name and don't do RHI_API in here, make it runtime changeable.

    // NOTE: Maybe change the name to RHIDevice instead?
    class RHIRenderer {
    public:
        explicit RHIRenderer(RHI_API& api)
            :mAPI(api) {}

        GPUBufferHandle CreateBuffer(const GPUBufferDesc& desc);
        Texture2DHandle CreateTexture2D(const TextureDesc& desc);
        TextureCubeHandle CreateTextureCube(const TextureDesc& desc);

        void Destroy(GPUBufferHandle handle);
        void Destroy(Texture2DHandle handle);
        void Destroy(TextureCubeHandle handle);

        bool UploadBuffer(GPUBufferHandle handle, Bytes data, uint64_t offset = 0);
        bool UploadTexture2D(Texture2DHandle handle, Bytes data);
        bool UploadTextureCube(TextureCubeHandle handle, const CubeFaces& faces);

        // Returns 0 if the handle is dead.
        uint64_t GetBufferDeviceAddress(GPUBufferHandle handle) const;

        // Returns 0 (the fallback texture) if the handle is dead.
        uint32_t GetBindlessIndex(Texture2DHandle handle) const;
        // Returns 0 (the fallback texture) if the handle is dead.
        uint32_t GetBindlessIndex(TextureCubeHandle handle) const;

    private:
        RHI_API& mAPI;
    };

}
