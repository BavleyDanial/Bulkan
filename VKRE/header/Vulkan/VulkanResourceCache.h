#pragma once

#include <Vulkan/VulkanContext.h>

#include <Vulkan/VulkanDescriptors.h>
#include <Vulkan/VulkanGPUBuffer.h>
#include <Vulkan/VulkanPipeline.h>
#include <Vulkan/VulkanImage.h>

#include <ResourceManager/ResourceHandles.h>
#include <ResourceManager/ResourcePool.h>
#include <ResourceManager/Resources.h>
#include <Renderer/RHIRenderer.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace VKRE {

    class VulkanResourceCache {
    public:
        VulkanResourceCache(VulkanContext& context, const RHIRendererSettings& settings, uint32_t framesInFlight);
        ~VulkanResourceCache();

        VulkanResourceCache(const VulkanResourceCache&) = delete;
        VulkanResourceCache& operator=(const VulkanResourceCache&) = delete;

        GPUBufferHandle CreateBuffer(const GPUBufferDesc& desc);
        Texture2DHandle CreateTexture2D(const TextureDesc& desc);
        TextureCubeHandle CreateTextureCube(const TextureDesc& desc);

        void Destroy(GPUBufferHandle handle);
        void Destroy(Texture2DHandle handle);
        void Destroy(TextureCubeHandle handle);

        bool UploadBuffer(GPUBufferHandle handle, Bytes data, uint64_t offset);
        bool UploadTexture2D(Texture2DHandle handle, Bytes data);
        bool UploadTextureCube(TextureCubeHandle handle, const CubeFaces& faces);

        uint64_t GetBufferDeviceAddress(GPUBufferHandle handle) const;
        uint32_t GetBindlessIndex(Texture2DHandle handle) const;
        uint32_t GetBindlessIndex(TextureCubeHandle handle) const;

        VulkanGPUBufferData* GetBufferData(GPUBufferHandle handle) { return mBuffers.Slots.Get(handle); }
        const VulkanGPUBufferData* GetBufferData(GPUBufferHandle handle) const { return mBuffers.Slots.Get(handle); }
        VulkanImageData* GetImageData2D(Texture2DHandle handle) { return mImages2D.Slots.Get(handle); }
        const VulkanImageData* GetImageData2D(Texture2DHandle handle) const { return mImages2D.Slots.Get(handle); }
        VulkanImageData* GetImageDataCube(TextureCubeHandle handle) { return mImagesCube.Slots.Get(handle); }
        const VulkanImageData* GetImageDataCube(TextureCubeHandle handle) const { return mImagesCube.Slots.Get(handle); }

        VkDescriptorSetLayout GetBindlessTexture2DLayout() const { return mBindlessTexture2DLayout; }
        VkDescriptorSetLayout GetBindlessTextureCubeLayout() const { return mBindlessTextureCubeLayout; }
        VkDescriptorSet GetBindlessTexture2DSet() const { return mBindlessTexture2DSet; }
        VkDescriptorSet GetBindlessTextureCubeSet() const { return mBindlessTextureCubeSet; }

        VkPipelineLayout CreatePipelineLayout(const VulkanPipelineLayoutKey& key);
        VkPipelineLayout GetPipelineLayout(const VulkanPipelineLayoutKey& key) const;

        VulkanGraphicsPipeline* CreateGraphicsPipeline(const VulkanGraphicsPipelineKey& key, const VulkanGraphicsShaders& shaders);
        VulkanGraphicsPipeline* GetGraphicsPipeline(const VulkanGraphicsPipelineKey& key);

        VulkanComputePipeline* CreateComputePipeline(const VulkanComputePipelineKey& key, const VulkanShaderStage& shader);
        VulkanComputePipeline* GetComputePipeline(const VulkanComputePipelineKey& key);

        void BeginFrame(uint64_t frameNumber);
        void DestroyAll();

    private:
        struct PendingDestroy {
            uint64_t Frame;
            uint32_t Index;
        };

        template<typename Tag, typename T>
        struct VulkanStore {
            ResourcePool<Tag, T> Slots;
            std::vector<PendingDestroy> PendingDestroys;
        };

        template<typename Tag, typename T>
        void DestroyLater(VulkanStore<Tag, T>& store, ResourceHandle<Tag> handle);

        template<typename Tag, typename T, typename Fn>
        void FreeFinished(VulkanStore<Tag, T>& store, Fn&& release);
        void FreeAllFinished();

        bool CreateImage(const TextureDesc& desc, bool isCube, VulkanImageData& outImage);
        VkShaderModule CreateShaderModule(const VulkanShaderStage& stage);

        void CreateDefaultSampler();
        void CreateBindlessTables(const RHIRendererSettings& settings);
        void CreateFallbackTextures();
        void WriteBindless(VkDescriptorSet set, uint32_t index, VkImageView view);
        void WriteBindlessAll(VkDescriptorSet set, uint32_t count, VkImageView view);

    private:
        VulkanContext& mContext;
        std::unique_ptr<class VulkanUploader> mUploader;

        uint64_t mFrameNumber = 0;
        uint32_t mFramesInFlight = 2;

        VulkanStore<GPUBufferTag, VulkanGPUBufferData> mBuffers;
        VulkanStore<Texture2DTag, VulkanImageData> mImages2D;
        VulkanStore<TextureCubeTag, VulkanImageData> mImagesCube;

        Texture2DHandle mFallbackTexture2D;
        TextureCubeHandle mFallbackTextureCube;

        VkSampler mDefaultSampler = VK_NULL_HANDLE;
        VulkanFixedBindlessDescriptorAllocator mBindlessTexture2DAllocator;
        VulkanFixedBindlessDescriptorAllocator mBindlessTextureCubeAllocator;
        VkDescriptorSetLayout mBindlessTexture2DLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout mBindlessTextureCubeLayout = VK_NULL_HANDLE;
        VkDescriptorSet mBindlessTexture2DSet = VK_NULL_HANDLE;
        VkDescriptorSet mBindlessTextureCubeSet = VK_NULL_HANDLE;

        std::unordered_map<VulkanPipelineLayoutKey, VkPipelineLayout> mPipelineLayouts;
        std::unordered_map<VulkanComputePipelineKey, VulkanComputePipeline> mComputePipelines;
        std::unordered_map<VulkanGraphicsPipelineKey, VulkanGraphicsPipeline> mGraphicsPipelines;
    };

}
