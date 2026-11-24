#include <Vulkan/VulkanUploader.h>
#include <Vulkan/VulkanUtils.h>

#include <cassert>
#include <cstring>
#include <array>
#include <print>
#include <vector>

namespace VKRE {

    VulkanUploader::VulkanUploader(VulkanContext& context, VulkanRHI& cache)
        :mContext(context), mCache(cache) {

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = mContext.GetQueueFamilies().Get(QueueCapability::Graphics);
        VK_CHECK(vkCreateCommandPool(mContext.GetLogicalDevice().handle, &poolInfo, nullptr, &mImmediatePool));

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = mImmediatePool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(mContext.GetLogicalDevice().handle, &allocInfo, &mImmediateBuffer));

        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(mContext.GetLogicalDevice().handle, &fenceCreateInfo, nullptr, &mImmediateFence));
    }

    VulkanUploader::~VulkanUploader() {
        VkDevice device = mContext.GetLogicalDevice().handle;
        vkDestroyCommandPool(mContext.GetLogicalDevice().handle, mImmediatePool, nullptr);
        vkDestroyFence(mContext.GetLogicalDevice().handle, mImmediateFence, nullptr);

        for (auto& buffer : mPendingStagingBuffers)
            GPUBufferUtils::ReleaseBuffer(mContext, &buffer);
        mPendingStagingBuffers.clear();
    }

    void VulkanUploader::UploadBuffer(GPUBufferHandle handle, const void* data, uint64_t size, uint64_t offset) {
        assert(mRecording && "VulkanUploader::UploadBuffer called without without Begin");

        VulkanGPUBufferData* dst = mCache.GetBufferData(handle);
        if (!dst) {
            std::println("VulkanUploader::UploadBuffer buffer not found (index={})", handle.Index);
            return;
        }

        VmaAllocationCreateInfo stagingAllocInfo{};
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        mPendingStagingBuffers.emplace_back(GPUBufferUtils::CreateBuffer(mContext, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingAllocInfo));
        memcpy(mPendingStagingBuffers.back().info.pMappedData, data, size);

        VkBufferCopy copy { .srcOffset = 0, .dstOffset = offset, .size = size };
        vkCmdCopyBuffer(mImmediateBuffer, mPendingStagingBuffers.back().buffer, dst->buffer, 1, &copy);
    }

    void VulkanUploader::UploadTexture2D(Texture2DHandle handle, const void* data, uint64_t size) {
        assert(mRecording && "VulkanUploader::UploadTexture called without without Begin");

        VulkanImageData* dst = mCache.GetImageData2D(handle);
        if (!dst) {
            std::println("VulkanUploader::UploadTexture texture not found (index={})", handle.Index);
            return;
        }
        VkImage targetImage = dst->image;
        VkExtent3D targetExtent = dst->extent;

        VmaAllocationCreateInfo stagingAllocInfo{};
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        mPendingStagingBuffers.emplace_back(GPUBufferUtils::CreateBuffer(mContext, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingAllocInfo));
        memcpy(mPendingStagingBuffers.back().info.pMappedData, data, size);

        ImageUtils::TransitionImage(mImmediateBuffer, targetImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // TODO: make all of this based on incoming data
        VkBufferImageCopy copy{};
        copy.bufferOffset = 0;
        copy.bufferRowLength = 0;
        copy.bufferImageHeight = 0;
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.mipLevel = 0;
        copy.imageSubresource.baseArrayLayer = 0;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = targetExtent;

        vkCmdCopyBufferToImage(mImmediateBuffer, mPendingStagingBuffers.back().buffer, dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        if (dst->mipLevels <= 1) {
            ImageUtils::TransitionImage(mImmediateBuffer, dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            return;
        }

        ImageUtils::TransitionImageMip(mImmediateBuffer, targetImage, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        uint32_t srcWidth = targetExtent.width;
        uint32_t srcHeight = targetExtent.height;

        for (uint32_t mip = 1; mip < dst->mipLevels; mip++) {
            uint32_t dstWidth = glm::max(srcWidth / 2, 1u);
            uint32_t dstHeight = glm::max(srcHeight / 2, 1u);

            ImageUtils::TransitionImageMip(mImmediateBuffer, targetImage, mip, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            ImageUtils::CopyImage(mImmediateBuffer, targetImage, targetImage, { srcWidth, srcHeight }, { dstWidth, dstHeight }, mip - 1, mip);
            ImageUtils::TransitionImageMip(mImmediateBuffer, targetImage, mip, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            srcWidth = dstWidth;
            srcHeight = dstHeight;
        }

        ImageUtils::TransitionImage(mImmediateBuffer, targetImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void VulkanUploader::Begin() {
        assert(!mRecording && "VulkanUploader::Begin called without matching End");
        VkDevice device = mContext.GetLogicalDevice().handle;

        VK_CHECK(vkWaitForFences(device, 1, &mImmediateFence, VK_TRUE, UINT64_MAX));
        VK_CHECK(vkResetFences(device, 1, &mImmediateFence));
        vkResetCommandBuffer(mImmediateBuffer, 0);

        VkCommandBufferBeginInfo cmdBufferBeginInfo{};
        cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBufferBeginInfo.pNext = nullptr;
        cmdBufferBeginInfo.pInheritanceInfo = nullptr;
        cmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(mImmediateBuffer, &cmdBufferBeginInfo));
        mRecording = true;
    }

    void VulkanUploader::UploadTextureCube(TextureCubeHandle handle, const CubeFaces& faces) {
        assert(mRecording && "VulkanUploader::UploadTextureCube called without Begin");

        VulkanImageData* dst = mCache.GetImageDataCube(handle);
        if (!dst) {
            std::println("VulkanUploader::UploadTextureCube texture not found (index={})", handle.Index);
            return;
        }

        for (const Bytes& face : faces) {
            if (face.size() != faces[0].size()) {
                std::println("VulkanUploader::UploadTextureCube faces have different sizes (index={})", handle.Index);
                return;
            }
        }

        uint64_t totalSize = 0;
        for (size_t i = 0; i < faces.size(); i++)
            totalSize += faces[i].size();

        VmaAllocationCreateInfo stagingAllocInfo{};
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        mPendingStagingBuffers.emplace_back(GPUBufferUtils::CreateBuffer(mContext, totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingAllocInfo));
        VulkanGPUBufferData& staging = mPendingStagingBuffers.back();

        std::vector<VkBufferImageCopy> copies;
        copies.reserve(6);

        for (uint32_t face = 0; face < 6; face++) {
            uint64_t faceSize = faces[face].size();
            memcpy(static_cast<std::byte*>(staging.info.pMappedData) + face * faceSize, faces[face].data(), faceSize);

            VkBufferImageCopy copy{};
            copy.bufferOffset = face * faceSize;
            copy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, face, 1 };
            copy.imageExtent = dst->extent;
            copies.push_back(copy);
        }

        ImageUtils::TransitionImage(mImmediateBuffer, dst->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        vkCmdCopyBufferToImage(mImmediateBuffer, staging.buffer, dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                static_cast<uint32_t>(copies.size()), copies.data());
        ImageUtils::TransitionImage(mImmediateBuffer, dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void VulkanUploader::End() {
        assert(mRecording && "VulkanUploader::End called without matching Begin");

        VkMemoryBarrier2 memoryBarrier{};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT | VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT;

        VkDependencyInfo dependencyInfo{};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.memoryBarrierCount = 1;
        dependencyInfo.pMemoryBarriers = &memoryBarrier;

        vkCmdPipelineBarrier2(mImmediateBuffer, &dependencyInfo);
        VK_CHECK(vkEndCommandBuffer(mImmediateBuffer));

        VkCommandBufferSubmitInfo cmdSubmitInfo;
        cmdSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        cmdSubmitInfo.pNext = nullptr;
        cmdSubmitInfo.commandBuffer = mImmediateBuffer;
        cmdSubmitInfo.deviceMask = 0;

        VkSubmitInfo2 info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        info.pNext = nullptr;
        info.commandBufferInfoCount = 1;
        info.pCommandBufferInfos = &cmdSubmitInfo;

        VK_CHECK(vkQueueSubmit2(mContext.GetGraphicsQueue(), 1, &info, mImmediateFence));
        VK_CHECK(vkWaitForFences(mContext.GetLogicalDevice().handle, 1, &mImmediateFence, true, UINT64_MAX));

        for (auto& buffer : mPendingStagingBuffers)
            GPUBufferUtils::ReleaseBuffer(mContext, &buffer);
        mPendingStagingBuffers.clear();

        mRecording = false;
    }

}
