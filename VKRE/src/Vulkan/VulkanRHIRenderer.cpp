#include <Renderer/RHIRenderer.h>
#include <Vulkan/VulkanResourceCache.h>

namespace VKRE {

    GPUBufferHandle RHIRenderer::CreateBuffer(const GPUBufferDesc& desc) {
        return mAPI.CreateBuffer(desc);
    }

    Texture2DHandle RHIRenderer::CreateTexture2D(const TextureDesc& desc) {
        return mAPI.CreateTexture2D(desc);
    }

    TextureCubeHandle RHIRenderer::CreateTextureCube(const TextureDesc& desc) {
        return mAPI.CreateTextureCube(desc);
    }

    void RHIRenderer::Destroy(GPUBufferHandle handle) {
        mAPI.Destroy(handle);
    }

    void RHIRenderer::Destroy(Texture2DHandle handle) {
        mAPI.Destroy(handle);
    }

    void RHIRenderer::Destroy(TextureCubeHandle handle) {
        mAPI.Destroy(handle);
    }

    bool RHIRenderer::UploadBuffer(GPUBufferHandle handle, Bytes data, uint64_t offset) {
        return mAPI.UploadBuffer(handle, data, offset);
    }

    bool RHIRenderer::UploadTexture2D(Texture2DHandle handle, Bytes data) {
        return mAPI.UploadTexture2D(handle, data);
    }

    bool RHIRenderer::UploadTextureCube(TextureCubeHandle handle, const CubeFaces& faces) {
        return mAPI.UploadTextureCube(handle, faces);
    }

    uint64_t RHIRenderer::GetBufferDeviceAddress(GPUBufferHandle handle) const {
        return mAPI.GetBufferDeviceAddress(handle);
    }

    uint32_t RHIRenderer::GetBindlessIndex(Texture2DHandle handle) const {
        return mAPI.GetBindlessIndex(handle);
    }

    uint32_t RHIRenderer::GetBindlessIndex(TextureCubeHandle handle) const {
        return mAPI.GetBindlessIndex(handle);
    }

}
