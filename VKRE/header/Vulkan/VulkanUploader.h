#pragma once

#include <Vulkan/VulkanContext.h>
#include <Vulkan/VulkanResourceCache.h>

#include <vector>

namespace VKRE {

    class VulkanUploader {
    public:
        VulkanUploader(VulkanContext& context, VulkanResourceCache& cache);
        ~VulkanUploader();

        void UploadBuffer(GPUBufferHandle handle, const void* data, uint64_t size, uint64_t offset);
        void UploadTexture2D(Texture2DHandle handle, const void* data, uint64_t size);
        void UploadTextureCube(TextureCubeHandle handle, const CubeFaces& faces);

        void Begin();
        void End();

    private:
        VulkanContext& mContext;
        VulkanResourceCache& mCache;

        VkCommandPool mImmediatePool;
        VkCommandBuffer mImmediateBuffer;
        VkFence mImmediateFence;

        std::vector<VulkanGPUBufferData> mPendingStagingBuffers;
        bool mRecording = false;
    };

}
