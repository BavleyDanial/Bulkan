#include <Vulkan/VulkanComputePass.h>

namespace VKRE {

    VulkanComputePass::VulkanComputePass(VulkanRHI& cache, const VulkanComputePipelineKey& key, VkDescriptorSet descriptorSet, const glm::vec3& workgroup, ComputePassStep step)
        :mPipeline(cache.GetComputePipeline(key)), mDescriptorSet(descriptorSet), mWorkGroup(workgroup), mStep(step) {}

    void VulkanComputePass::Execute(VkCommandBuffer cmd, VkExtent2D extent) {
        Execute(cmd, { extent.width, extent.height, 1 });
    }

    void VulkanComputePass::Execute(VkCommandBuffer cmd, VkExtent3D extent) {
        if (!mPipeline) return;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline->pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline->layout, 0, 1, &mDescriptorSet, 0, nullptr);

        if (!mPushConstantData.empty()) {
            vkCmdPushConstants(cmd, mPipeline->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, mPushConstantData.size(), mPushConstantData.data());
        }

        vkCmdDispatch(cmd,
            static_cast<uint32_t>(glm::ceil(extent.width  / static_cast<float>(mWorkGroup.x))),
            static_cast<uint32_t>(glm::ceil(extent.height / static_cast<float>(mWorkGroup.y))),
            static_cast<uint32_t>(glm::ceil(extent.depth  / static_cast<float>(mWorkGroup.z)))
        );
    }

    void VulkanComputePass::SetPushConstantData(const void* data, uint32_t size) {
        mPushConstantData.resize(size);
        memcpy(mPushConstantData.data(), data, size);
    }

    void VulkanComputePass::ReBuild(VkDescriptorSet newDescriptorSet) {
        mDescriptorSet = newDescriptorSet;
    }

}
