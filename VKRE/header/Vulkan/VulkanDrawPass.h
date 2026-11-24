#pragma once

#include <Vulkan/VulkanRHI.h>
#include <Vulkan/VulkanDescriptors.h>

#include <Renderer/RendererCommands.h>

#include <glm/glm.hpp>
#include <optional>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace VKRE {

    struct RenderTargetInfo {
        std::vector<VkRenderingAttachmentInfo> colorAttachments;
        VkRenderingAttachmentInfo* depthAttachment = nullptr;
        VkRenderingAttachmentInfo* stencilAttachment = nullptr;
    };

    class VulkanDrawPass {
    public:
        VulkanDrawPass(VkDevice device, VulkanRHI& cache, const VulkanGraphicsPipelineKey& key, VkDescriptorSet descriptorSet,
                        VkShaderStageFlags pushConstantsShaderStages);

        void SetActive(bool enabled) { mIsActive = enabled; }
        bool IsActive() const { return mIsActive; }

        void SetColorLoadOp(VkAttachmentLoadOp op) { mColorLoadOp = op; }
        VkAttachmentLoadOp GetColorLoadOp() const { return mColorLoadOp; }
        void SetDepthLoadOp(VkAttachmentLoadOp op) { mDepthLoadOp = op; }
        VkAttachmentLoadOp GetDepthLoadOp() const { return mDepthLoadOp; }

        void SubmitMeshDraw(const MeshDrawCommand& command);
        void SubmitSkyboxDraw(const SkyboxDrawCommand& command);
        VkPipelineLayout GetPipelineLayout() const { return mPipeline->layout; }

        void ReBuild(VkDescriptorSet newDescriptorSet);
        void Execute(VkCommandBuffer cmd, VkExtent2D extent, const RenderTargetInfo& targetInfo,
                    VkDescriptorSet sceneSet, VkDescriptorSet bindlessTexture2DSet, VkDescriptorSet bindlessTextureCubeSet);
    private:
        VkDevice mDevice;
        VulkanRHI& mCache;

        VulkanGraphicsPipeline* mPipeline;
        VkDescriptorSet mDescriptorSet;
        VkShaderStageFlags mPushConstantsShaderStages;
        std::vector<MeshDrawCommand> mDrawCommands;
        std::optional<SkyboxDrawCommand> mSkyboxCommand;

        bool mIsActive = true;
        VkAttachmentLoadOp mColorLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        VkAttachmentLoadOp mDepthLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    };

}
