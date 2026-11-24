#include "Renderer/RendererCommands.h"
#include <Vulkan/VulkanDrawPass.h>

namespace VKRE {

    VulkanDrawPass::VulkanDrawPass(VkDevice device, VulkanResourceCache& cache, const VulkanGraphicsPipelineKey& key,
                                    VkDescriptorSet descriptorSet, VkShaderStageFlags pushConstantsShaderStages)
        :mDevice(device), mCache(cache), mPipeline(cache.GetGraphicsPipeline(key)),
        mDescriptorSet(descriptorSet), mPushConstantsShaderStages(pushConstantsShaderStages) {}

    void VulkanDrawPass::Execute(VkCommandBuffer cmd, VkExtent2D extent, const RenderTargetInfo& targetInfo,
                                    VkDescriptorSet sceneSet, VkDescriptorSet bindlessTexture2DSet, VkDescriptorSet bindlessTextureCubeSet) {
        if (!mPipeline || !mIsActive) return;

        VkRenderingInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        info.renderArea = { { 0, 0 }, extent };
        info.layerCount = 1;
        info.colorAttachmentCount = static_cast<uint32_t>(targetInfo.colorAttachments.size());
        info.pColorAttachments = static_cast<uint32_t>(targetInfo.colorAttachments.size()) ? targetInfo.colorAttachments.data() : nullptr;
        info.pDepthAttachment = targetInfo.depthAttachment;
        info.pStencilAttachment = targetInfo.stencilAttachment;

        vkCmdBeginRendering(cmd, &info);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline->pipeline);

        if (mDescriptorSet != VK_NULL_HANDLE)
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline->layout, 0, 1, &mDescriptorSet, 0, nullptr);
        if (sceneSet != VK_NULL_HANDLE)
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline->layout, 1, 1, &sceneSet, 0, nullptr);
        if (bindlessTexture2DSet != VK_NULL_HANDLE)
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline->layout, 2, 1, &bindlessTexture2DSet, 0, nullptr);
        if (bindlessTextureCubeSet != VK_NULL_HANDLE)
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline->layout, 3, 1, &bindlessTextureCubeSet, 0, nullptr);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = static_cast<float>(extent.height);
        viewport.width = static_cast<float>(extent.width);
        viewport.height = -static_cast<float>(extent.height); 
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = extent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        VkBuffer lastIB = VK_NULL_HANDLE;
        VulkanDescriptorWriter writer;
        for (const auto& draw : mDrawCommands) {
            const VulkanGPUBufferData* indexBuffer = mCache.GetBufferData(draw.IndexBuffer);
            if (!indexBuffer)
                continue;

            DrawPushConstants pushConstants{};
            pushConstants.VertexBufferAddress = draw.VertexBufferAddress;
            pushConstants.Transform = draw.Transform;
            pushConstants.Texture2DIndex = draw.TextureIndex;
            vkCmdPushConstants(cmd, mPipeline->layout, mPushConstantsShaderStages, 0, sizeof(DrawPushConstants), &pushConstants);

            VkBuffer ib = indexBuffer->buffer;
            if (ib != lastIB) {
                vkCmdBindIndexBuffer(cmd, ib, 0, VK_INDEX_TYPE_UINT32);
                lastIB = ib;
            }

            vkCmdDrawIndexed(cmd, draw.IndexCount, 1, draw.BaseIndex, draw.BaseVertex, 0);
        }

        if (mSkyboxCommand.has_value()) {
            const SkyboxPushConstants& pushConstants = mSkyboxCommand.value().PushConstants;
            vkCmdPushConstants(cmd, mPipeline->layout, mPushConstantsShaderStages, 0, sizeof(SkyboxPushConstants), &pushConstants);
            vkCmdDraw(cmd, 36, 1, 0, 0);
        }

        vkCmdEndRendering(cmd);
        mDrawCommands.clear();
        mSkyboxCommand.reset();
    }

    void VulkanDrawPass::SubmitMeshDraw(const MeshDrawCommand& command) {
        mDrawCommands.push_back(command);
    }

    void VulkanDrawPass::SubmitSkyboxDraw(const SkyboxDrawCommand& command) {
        mSkyboxCommand = command;
    }

    void VulkanDrawPass::ReBuild(VkDescriptorSet newDescriptorSet) {
        mDescriptorSet = newDescriptorSet;
    }

}
