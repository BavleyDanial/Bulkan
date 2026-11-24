#include <Vulkan/VulkanResourceCache.h>

#include <Vulkan/VulkanUploader.h>
#include <Vulkan/VulkanUtils.h>
#include <Vulkan/VulkanPipelineBuilder.h>

#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <print>
#include <vector>

namespace VKRE {

    static VkFormat ToVkTextureFormat(TextureFormat format) {
        switch (format) {
            case TextureFormat::R8G8B8A8_SRGB:          return VK_FORMAT_R8G8B8A8_SRGB;
            case TextureFormat::R8G8B8A8_UNORM:         return VK_FORMAT_R8G8B8A8_UNORM;
            case TextureFormat::B8G8R8A8_SRGB:          return VK_FORMAT_B8G8R8A8_SRGB;
            case TextureFormat::B8G8R8A8_UNORM:         return VK_FORMAT_B8G8R8A8_UNORM;
            case TextureFormat::R16G16B16A16_SFLOAT:    return VK_FORMAT_R16G16B16A16_SFLOAT;
            case TextureFormat::R32G32B32A32_SFLOAT:    return VK_FORMAT_R32G32B32A32_SFLOAT;
            case TextureFormat::R16G16_SFLOAT:          return VK_FORMAT_R16G16_SFLOAT;
            case TextureFormat::R8G8_UNORM:             return VK_FORMAT_R8G8_UNORM;
            case TextureFormat::R32_UINT:               return VK_FORMAT_R32_UINT;
            case TextureFormat::D32_SFLOAT:             return VK_FORMAT_D32_SFLOAT;
            case TextureFormat::D24_UNORM_S8_UINT:      return VK_FORMAT_D24_UNORM_S8_UINT;
            case TextureFormat::BC1_RGB_SRGB:           return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
            case TextureFormat::BC1_RGB_UNORM:          return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
            case TextureFormat::BC3_RGBA_SRGB:          return VK_FORMAT_BC3_SRGB_BLOCK;
            case TextureFormat::BC5_UNORM:              return VK_FORMAT_BC5_UNORM_BLOCK;
            case TextureFormat::BC7_RGBA_SRGB:          return VK_FORMAT_BC7_SRGB_BLOCK;
            case TextureFormat::BC7_RGBA_UNORM:         return VK_FORMAT_BC7_UNORM_BLOCK;
            default:                                    return VK_FORMAT_UNDEFINED;
        }
    }

    static VkImageAspectFlags VkGetAspectFlags(TextureFormat format) {
        switch (format) {
            case TextureFormat::D32_SFLOAT:         return VK_IMAGE_ASPECT_DEPTH_BIT;
            case TextureFormat::D24_UNORM_S8_UINT:  return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            default:                                return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    static VkImageUsageFlags ToVkImageUsage(TextureUsage usage) {
        VkImageUsageFlags flags = 0;
        if ((usage & TextureUsage::Sampled) == TextureUsage::Sampled)                               flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        if ((usage & TextureUsage::ColorAttachment) == TextureUsage::ColorAttachment)               flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if ((usage & TextureUsage::DepthStencilAttachment) == TextureUsage::DepthStencilAttachment) flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if ((usage & TextureUsage::InputAttachment) == TextureUsage::InputAttachment)               flags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        if ((usage & TextureUsage::Storage) == TextureUsage::Storage)                               flags |= VK_IMAGE_USAGE_STORAGE_BIT;
        if ((usage & TextureUsage::TransferSrc) == TextureUsage::TransferSrc)                       flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if ((usage & TextureUsage::TransferDst) == TextureUsage::TransferDst)                       flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        return flags;
    }

    static VkBufferUsageFlags ToVkBufferUsage(GPUBufferUsage usage) {
        VkBufferUsageFlags flags = 0;
        if ((usage & GPUBufferUsage::Vertex) == GPUBufferUsage::Vertex)           flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if ((usage & GPUBufferUsage::Index) == GPUBufferUsage::Index)             flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if ((usage & GPUBufferUsage::Uniform) == GPUBufferUsage::Uniform)         flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if ((usage & GPUBufferUsage::Storage) == GPUBufferUsage::Storage)         flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if ((usage & GPUBufferUsage::Indirect) == GPUBufferUsage::Indirect)       flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        if ((usage & GPUBufferUsage::TransferSrc) == GPUBufferUsage::TransferSrc) flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if ((usage & GPUBufferUsage::TransferDst) == GPUBufferUsage::TransferDst) flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        return flags;
    }

    VulkanResourceCache::VulkanResourceCache(VulkanContext& context, const RHIRendererSettings& settings, uint32_t framesInFlight)
        :mContext(context), mFramesInFlight(framesInFlight) {
        mBuffers.Slots.Init(settings.MaxBuffers);
        mImages2D.Slots.Init(settings.MaxTextures2D);
        mImagesCube.Slots.Init(settings.MaxTextureCubes);

        mUploader = std::make_unique<VulkanUploader>(mContext, *this);

        CreateDefaultSampler();
        CreateBindlessTables(settings);
        CreateFallbackTextures(); // must be the first textures created so the fallbacks get slot 0
    }


    VulkanResourceCache::~VulkanResourceCache() = default;

    GPUBufferHandle VulkanResourceCache::CreateBuffer(const GPUBufferDesc& desc) {
        if (desc.Size == 0) {
            std::println("VulkanResourceCache::CreateBuffer size is 0 ({})", desc.DebugName);
            return GPUBufferHandle::Null();
        }

        GPUBufferHandle handle = mBuffers.Slots.Allocate();
        if (handle.IsNull()) {
            std::println("VulkanResourceCache::CreateBuffer buffer pool is full ({})", desc.DebugName);
            return GPUBufferHandle::Null();
        }

        VkBufferUsageFlags vkUsage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | ToVkBufferUsage(desc.Usage);

        VmaAllocationCreateInfo allocInfo{};
        if (desc.HostVisible) {
            allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        } else {
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            vkUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        *mBuffers.Slots.Get(handle) = GPUBufferUtils::CreateBuffer(mContext, desc.Size, vkUsage, allocInfo);
        return handle;
    }

    bool VulkanResourceCache::CreateImage(const TextureDesc& desc, bool isCube, VulkanImageData& outImage) {
        VkFormat vkFormat = ToVkTextureFormat(desc.Format);
        if (vkFormat == VK_FORMAT_UNDEFINED) {
            std::println("VulkanResourceCache::CreateImage format is not supported ({})", desc.DebugName);
            return false;
        }

        if (desc.Width == 0 || desc.Height == 0) {
            std::println("VulkanResourceCache::CreateImage size is 0 ({})", desc.DebugName);
            return false;
        }

        VkImageUsageFlags vkUsage = ToVkImageUsage(desc.Usage);
        VkExtent3D extent = { desc.Width, desc.Height, 1 };
        VkImageAspectFlags vkAspect = VkGetAspectFlags(desc.Format);
        uint32_t mipLevels = desc.MipLevels > 0 ? desc.MipLevels : 1;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        if (isCube)
            outImage = ImageUtils::CreateImageCube(mContext, vkFormat, vkUsage, extent, vkAspect, mipLevels, allocInfo);
        else
            outImage = ImageUtils::CreateImage(mContext, vkFormat, vkUsage, extent, vkAspect, mipLevels, allocInfo);
        return true;
    }

    Texture2DHandle VulkanResourceCache::CreateTexture2D(const TextureDesc& desc) {
        Texture2DHandle handle = mImages2D.Slots.Allocate();
        if (handle.IsNull()) {
            std::println("VulkanResourceCache::CreateTexture2D texture pool is full ({})", desc.DebugName);
            return Texture2DHandle::Null();
        }

        if (!CreateImage(desc, false, *mImages2D.Slots.Get(handle))) {
            mImages2D.Slots.Free(handle);
            return Texture2DHandle::Null();
        }
        return handle;
    }

    TextureCubeHandle VulkanResourceCache::CreateTextureCube(const TextureDesc& desc) {
        TextureCubeHandle handle = mImagesCube.Slots.Allocate();
        if (handle.IsNull()) {
            std::println("VulkanResourceCache::CreateTextureCube texture pool is full ({})", desc.DebugName);
            return TextureCubeHandle::Null();
        }

        if (!CreateImage(desc, true, *mImagesCube.Slots.Get(handle))) {
            mImagesCube.Slots.Free(handle);
            return TextureCubeHandle::Null();
        }
        return handle;
    }

    template<typename Tag, typename T>
    void VulkanResourceCache::DestroyLater(VulkanStore<Tag, T>& store, ResourceHandle<Tag> handle) {
        if (!store.Slots.Retire(handle))
            return; // null or already destroyed
        store.PendingDestroys.push_back({ mFrameNumber, handle.Index });
    }

    void VulkanResourceCache::Destroy(GPUBufferHandle handle) {
        DestroyLater(mBuffers, handle);
    }

    void VulkanResourceCache::Destroy(Texture2DHandle handle) {
        if (handle == mFallbackTexture2D) return;
        DestroyLater(mImages2D, handle);
    }

    void VulkanResourceCache::Destroy(TextureCubeHandle handle) {
        if (handle == mFallbackTextureCube) return;
        DestroyLater(mImagesCube, handle);
    }

    template<typename Tag, typename T, typename Fn>
    void VulkanResourceCache::FreeFinished(VulkanStore<Tag, T>& store, Fn&& release) {
        std::vector<PendingDestroy>& pending = store.PendingDestroys;
        size_t kept = 0;
        for (const PendingDestroy& entry : pending) {
            if (entry.Frame + mFramesInFlight <= mFrameNumber) {
                release(entry.Index, store.Slots.At(entry.Index));
                store.Slots.Recycle(entry.Index);
            } else {
                pending[kept++] = entry;
            }
        }
        pending.resize(kept);
    }

    void VulkanResourceCache::FreeAllFinished() {
        FreeFinished(mBuffers, [&](uint32_t, VulkanGPUBufferData& buffer) {
            GPUBufferUtils::ReleaseBuffer(mContext, &buffer);
        });

        VkImageView fallback2D = mImages2D.Slots.Get(mFallbackTexture2D)->imageView;
        FreeFinished(mImages2D, [&](uint32_t index, VulkanImageData& image) {
            WriteBindless(mBindlessTexture2DSet, index, fallback2D);
            ImageUtils::ReleaseImage(mContext, &image);
        });

        VkImageView fallbackCube = mImagesCube.Slots.Get(mFallbackTextureCube)->imageView;
        FreeFinished(mImagesCube, [&](uint32_t index, VulkanImageData& image) {
            WriteBindless(mBindlessTextureCubeSet, index, fallbackCube);
            ImageUtils::ReleaseImage(mContext, &image);
        });
    }

    void VulkanResourceCache::BeginFrame(uint64_t frameNumber) {
        mFrameNumber = frameNumber;
        FreeAllFinished();
    }

    void VulkanResourceCache::DestroyAll() {
        mFrameNumber = std::numeric_limits<uint64_t>::max() - mFramesInFlight;
        FreeAllFinished();

        uint32_t leakedBuffers = 0;
        mBuffers.Slots.ForEach([&](GPUBufferHandle, VulkanGPUBufferData& buffer) {
            GPUBufferUtils::ReleaseBuffer(mContext, &buffer);
            leakedBuffers++;
        });

        uint32_t leakedTextures = 0;
        mImages2D.Slots.ForEach([&](Texture2DHandle handle, VulkanImageData& image) {
            ImageUtils::ReleaseImage(mContext, &image);
            if (handle != mFallbackTexture2D) leakedTextures++;
        });
        mImagesCube.Slots.ForEach([&](TextureCubeHandle handle, VulkanImageData& image) {
            ImageUtils::ReleaseImage(mContext, &image);
            if (handle != mFallbackTextureCube) leakedTextures++;
        });

        if (leakedBuffers > 0 || leakedTextures > 0)
            std::println("VulkanResourceCache::DestroyAll {} buffer(s) and {} texture(s) were never destroyed", leakedBuffers, leakedTextures);

        mUploader.reset();

        VkDevice device = mContext.GetLogicalDevice().handle;

        for (auto& [key, pipeline] : mGraphicsPipelines)
            pipeline.Destroy(device);
        mGraphicsPipelines.clear();

        for (auto& [key, pipeline] : mComputePipelines)
            pipeline.Destroy(device);
        mComputePipelines.clear();

        for (auto& [key, layout] : mPipelineLayouts)
            vkDestroyPipelineLayout(device, layout, nullptr);
        mPipelineLayouts.clear();

        mBindlessTexture2DAllocator.DestroyPool(device);
        mBindlessTextureCubeAllocator.DestroyPool(device);
        vkDestroyDescriptorSetLayout(device, mBindlessTexture2DLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, mBindlessTextureCubeLayout, nullptr);
        vkDestroySampler(device, mDefaultSampler, nullptr);
    }

    bool VulkanResourceCache::UploadBuffer(GPUBufferHandle handle, Bytes data, uint64_t offset) {
        VulkanGPUBufferData* buffer = mBuffers.Slots.Get(handle);
        if (!buffer) {
            std::println("VulkanResourceCache::UploadBuffer dead handle (index={})", handle.Index);
            return false;
        }

        if (offset + data.size() > buffer->info.size) {
            std::println("VulkanResourceCache::UploadBuffer {} bytes at offset {} don't fit in {} bytes", data.size(), offset, buffer->info.size);
            return false;
        }

        if (buffer->info.pMappedData) {
            memcpy(static_cast<std::byte*>(buffer->info.pMappedData) + offset, data.data(), data.size());
            return true;
        }

        mUploader->Begin();
        mUploader->UploadBuffer(handle, data.data(), data.size(), offset);
        mUploader->End();
        return true;
    }

    bool VulkanResourceCache::UploadTexture2D(Texture2DHandle handle, Bytes data) {
        VulkanImageData* image = mImages2D.Slots.Get(handle);
        if (!image) {
            std::println("VulkanResourceCache::UploadTexture2D dead handle (index={})", handle.Index);
            return false;
        }

        mUploader->Begin();
        mUploader->UploadTexture2D(handle, data.data(), data.size());
        mUploader->End();

        WriteBindless(mBindlessTexture2DSet, handle.Index, image->imageView);
        return true;
    }

    bool VulkanResourceCache::UploadTextureCube(TextureCubeHandle handle, const CubeFaces& faces) {
        VulkanImageData* image = mImagesCube.Slots.Get(handle);
        if (!image) {
            std::println("VulkanResourceCache::UploadTextureCube dead handle (index={})", handle.Index);
            return false;
        }

        mUploader->Begin();
        mUploader->UploadTextureCube(handle, faces);
        mUploader->End();

        WriteBindless(mBindlessTextureCubeSet, handle.Index, image->imageView);
        return true;
    }

    uint64_t VulkanResourceCache::GetBufferDeviceAddress(GPUBufferHandle handle) const {
        const VulkanGPUBufferData* buffer = mBuffers.Slots.Get(handle);
        return buffer ? buffer->deviceAddress : 0;
    }

    uint32_t VulkanResourceCache::GetBindlessIndex(Texture2DHandle handle) const {
        return mImages2D.Slots.IsValid(handle) ? handle.Index : 0;
    }

    uint32_t VulkanResourceCache::GetBindlessIndex(TextureCubeHandle handle) const {
        return mImagesCube.Slots.IsValid(handle) ? handle.Index : 0;
    }

    VkShaderModule VulkanResourceCache::CreateShaderModule(const VulkanShaderStage& stage) {
        if (!stage.IsValid())
            return VK_NULL_HANDLE;

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = stage.spirV.size_bytes();
        createInfo.pCode = stage.spirV.data();

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        if (vkCreateShaderModule(mContext.GetLogicalDevice().handle, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            std::println("VulkanResourceCache::CreateShaderModule failed ({} words)", stage.spirV.size());
            return VK_NULL_HANDLE;
        }
        return shaderModule;
    }

    VkPipelineLayout VulkanResourceCache::CreatePipelineLayout(const VulkanPipelineLayoutKey& key) {
        auto it = mPipelineLayouts.find(key);
        if (it != mPipelineLayouts.end())
            return it->second;

        const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts = key.descriptorSetLayouts;
        const std::vector<VkPushConstantRange>& pushConstantRanges = key.pushConstantRanges;

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pSetLayouts = descriptorSetLayouts.empty() ? nullptr : descriptorSetLayouts.data();
        layoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        layoutInfo.pPushConstantRanges = pushConstantRanges.empty() ? nullptr : pushConstantRanges.data();
        layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());

        VkPipelineLayout layout = VK_NULL_HANDLE;
        if (vkCreatePipelineLayout(mContext.GetLogicalDevice().handle, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
            std::println("VulkanResourceCache::CreatePipelineLayout Failed to create Pipeline Layout");
            return VK_NULL_HANDLE;
        }

        mPipelineLayouts[key] = layout;
        return layout;
    }

    VkPipelineLayout VulkanResourceCache::GetPipelineLayout(const VulkanPipelineLayoutKey& key) const {
        auto it = mPipelineLayouts.find(key);
        if (it == mPipelineLayouts.end()) return VK_NULL_HANDLE;
        return it->second;
    }

    VulkanGraphicsPipeline* VulkanResourceCache::CreateGraphicsPipeline(const VulkanGraphicsPipelineKey& key, const VulkanGraphicsShaders& shaders) {
        auto it = mGraphicsPipelines.find(key);
        if (it != mGraphicsPipelines.end())
            return &it->second;

        assert(key.vertexShader == HashShaderStage(shaders.vertex) && key.fragmentShader == HashShaderStage(shaders.fragment)
            && "VulkanResourceCache::CreateGraphicsPipeline key doesn't match the shaders");

        VkDevice device = mContext.GetLogicalDevice().handle;

        VkShaderModule vertexModule = CreateShaderModule(shaders.vertex);
        VkShaderModule fragmentModule = CreateShaderModule(shaders.fragment);
        if (!vertexModule || !fragmentModule) {
            if (vertexModule) vkDestroyShaderModule(device, vertexModule, nullptr);
            if (fragmentModule) vkDestroyShaderModule(device, fragmentModule, nullptr);
            std::println("VulkanResourceCache::CreateGraphicsPipeline Failed to create a shader module");
            return nullptr;
        }

        VulkanGraphicsPipelineBuilder builder;
        builder.SetPipelineLayout(key.layout)
                .SetVertexShader(vertexModule, shaders.vertex.entryPoint)
                .SetFragmentShader(fragmentModule, shaders.fragment.entryPoint)
                .SetTopology(key.topology)
                .SetPrimitiveRestart(key.primitveRestartEnable)
                .SetPolygonMode(key.polygonMode)
                .SetCullMode(key.cullMode)
                .SetFrontFace(key.frontFace)
                .SetDepthClamp(key.depthClampEnable)
                .SetRasterDiscard(key.rasterDiscardEnable)
                .SetSamples(key.samples)
                .SetSampleShading(key.sampleShadingEnable)
                .SetDepthTest(key.depthTestEnable)
                .SetDepthWrite(key.depthWriteEnable)
                .SetDepthCompareOp(key.depthCompareOp)
                .SetStencilTest(key.stencilTestEnable)
                .SetBlendEnable(key.blendEnable)
                .SetColorBlend(key.srcColorBlendFactor, key.dstColorBlendFactor, key.colorBlendOp)
                .SetAlphaBlend(key.srcAlphaBlendFactor, key.dstAlphaBlendFactor, key.alphaBlendOp)
                .SetColorWriteMask(key.colorWriteMask)
                .SetColorAttachmentFormats(key.colorAttachmentFromats)
                .SetDepthAttachmentFormat(key.depthAttachmentFormat)
                .SetStencilAttachmentFormat(key.stencilAttachmentFormat);

        VulkanGraphicsPipeline pipeline = builder.Build(device);
        vkDestroyShaderModule(device, vertexModule, nullptr);
        vkDestroyShaderModule(device, fragmentModule, nullptr);

        if (!pipeline.Succeeded()) {
            std::println("VulkanResourceCache::CreateGraphicsPipeline Failed to create graphics pipeline");
            return nullptr;
        }

        auto [inserted, _] = mGraphicsPipelines.emplace(key, pipeline);
        return &inserted->second;
    }

    VulkanGraphicsPipeline* VulkanResourceCache::GetGraphicsPipeline(const VulkanGraphicsPipelineKey& key) {
        auto it = mGraphicsPipelines.find(key);
        if (it == mGraphicsPipelines.end()) return nullptr;
        return &it->second;
    }

    VulkanComputePipeline* VulkanResourceCache::CreateComputePipeline(const VulkanComputePipelineKey& key, const VulkanShaderStage& shader) {
        auto it = mComputePipelines.find(key);
        if (it != mComputePipelines.end())
            return &it->second;

        assert(key.shader == HashShaderStage(shader) && "VulkanResourceCache::CreateComputePipeline key doesn't match the shader");

        VkDevice device = mContext.GetLogicalDevice().handle;
        VkShaderModule shaderModule = CreateShaderModule(shader);
        if (!shaderModule) {
            std::println("VulkanResourceCache::CreateComputePipeline Failed to create a shader module");
            return nullptr;
        }

        VulkanComputePipeline pipeline = VulkanComputePipelineBuilder()
            .SetComputeShader(shaderModule, shader.entryPoint)
            .SetPipelineLayout(key.layout)
            .Build(device);
        vkDestroyShaderModule(device, shaderModule, nullptr);

        if (!pipeline.Succeeded()) {
            std::println("VulkanResourceCache::CreateComputePipeline Failed to create compute pipeline");
            return nullptr;
        }

        auto [inserted, _] = mComputePipelines.emplace(key, pipeline);
        return &inserted->second;
    }

    VulkanComputePipeline* VulkanResourceCache::GetComputePipeline(const VulkanComputePipelineKey& key) {
        auto it = mComputePipelines.find(key);
        if (it == mComputePipelines.end()) return nullptr;
        return &it->second;
    }

    void VulkanResourceCache::CreateDefaultSampler() {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        VK_CHECK(vkCreateSampler(mContext.GetLogicalDevice().handle, &samplerInfo, nullptr, &mDefaultSampler));
    }

    void VulkanResourceCache::CreateBindlessTables(const RHIRendererSettings& settings) {
        VkDevice device = mContext.GetLogicalDevice().handle;

        VkDescriptorBindingFlags bindingFlags =
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;

        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
        bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlagsInfo.bindingCount = 1;
        bindingFlagsInfo.pBindingFlags = &bindingFlags;

        {   // Texture2D Descriptor Set 2
            VulkanDescriptorLayoutBuilder layoutBuilder;
            layoutBuilder.AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, settings.MaxTextures2D);
            mBindlessTexture2DLayout = layoutBuilder.Build(device, VK_SHADER_STAGE_FRAGMENT_BIT,
                    &bindingFlagsInfo,
                    VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);

            mBindlessTexture2DAllocator.InitPool(device, settings.MaxTextures2D);
            mBindlessTexture2DSet = mBindlessTexture2DAllocator.Allocate(device, mBindlessTexture2DLayout);
        }

        {   // TextureCube Descriptor Set 3
            VulkanDescriptorLayoutBuilder layoutBuilder;
            layoutBuilder.AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, settings.MaxTextureCubes);
            mBindlessTextureCubeLayout = layoutBuilder.Build(device, VK_SHADER_STAGE_FRAGMENT_BIT,
                    &bindingFlagsInfo,
                    VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);

            mBindlessTextureCubeAllocator.InitPool(device, settings.MaxTextureCubes);
            mBindlessTextureCubeSet = mBindlessTextureCubeAllocator.Allocate(device, mBindlessTextureCubeLayout);
        }
    }

    void VulkanResourceCache::CreateFallbackTextures() {
        // 1x1 white 2D and 1x1 black cube. Being the first allocations, they land in slot 0.
        TextureDesc desc{};
        desc.Width = 1;
        desc.Height = 1;
        desc.Format = TextureFormat::R8G8B8A8_UNORM;
        desc.Usage = TextureUsage::Sampled | TextureUsage::TransferDst;
        desc.MipLevels = 1;

        desc.DebugName = "Fallback2D";
        mFallbackTexture2D = CreateTexture2D(desc);
        assert(mFallbackTexture2D.Index == 0 && "VulkanResourceCache fallback 2D texture must be slot 0");

        desc.DebugName = "FallbackCube";
        mFallbackTextureCube = CreateTextureCube(desc);
        assert(mFallbackTextureCube.Index == 0 && "VulkanResourceCache fallback cube texture must be slot 0");

        const std::array<std::byte, 4> white = { std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255} };
        const std::array<std::byte, 4> black = { std::byte{0}, std::byte{0}, std::byte{0}, std::byte{255} };
        UploadTexture2D(mFallbackTexture2D, white);
        UploadTextureCube(mFallbackTextureCube, { black, black, black, black, black, black });

        // Every slot starts on the fallback, so unused and dead indices are always safe to sample.
        WriteBindlessAll(mBindlessTexture2DSet, mImages2D.Slots.GetCapacity(), mImages2D.Slots.Get(mFallbackTexture2D)->imageView);
        WriteBindlessAll(mBindlessTextureCubeSet, mImagesCube.Slots.GetCapacity(), mImagesCube.Slots.Get(mFallbackTextureCube)->imageView);
    }

    void VulkanResourceCache::WriteBindless(VkDescriptorSet set, uint32_t index, VkImageView view) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = mDefaultSampler;
        imageInfo.imageView = view;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set;
        write.dstBinding = 0;
        write.dstArrayElement = index;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(mContext.GetLogicalDevice().handle, 1, &write, 0, nullptr);
    }

    void VulkanResourceCache::WriteBindlessAll(VkDescriptorSet set, uint32_t count, VkImageView view) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = mDefaultSampler;
        imageInfo.imageView = view;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        std::vector<VkDescriptorImageInfo> infos(count, imageInfo);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set;
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorCount = count;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = infos.data();

        vkUpdateDescriptorSets(mContext.GetLogicalDevice().handle, 1, &write, 0, nullptr);
    }

}
