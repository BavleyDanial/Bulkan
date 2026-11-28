#pragma once

#include <Vulkan/VulkanUtils.h>
#include <Vulkan/VulkanRHI.h>

#include <ResourceManager/ResourceHandles.h>
#include <glm/glm.hpp>

namespace VKRE {

    enum class ComputePassStep {
        PreDraw = 1,
        PostProcess
    };

    class VulkanComputePass {
    public:
        VulkanComputePass(VulkanRHI& cache, const VulkanComputePipelineKey& key, VkDescriptorSet descriptorSet, const glm::vec3& workgroup, ComputePassStep step);

        void SetPushConstantData(const void* data, uint32_t size);
        void SetActive(bool enabled) { mIsActive = enabled; }
        void ReBuild(VkDescriptorSet newDescriptorSet);

        bool IsActive() const { return mIsActive; }
        ComputePassStep GetActiveStep() const { return mStep; }

        void Execute(VkCommandBuffer cmd, VkExtent2D extent);
        void Execute(VkCommandBuffer cmd, VkExtent3D extent);

    private:
        VulkanComputePipeline* mPipeline;
        VkDescriptorSet mDescriptorSet;
        std::vector<uint8_t> mPushConstantData;
        glm::vec3 mWorkGroup;
        bool mIsActive = true;
        ComputePassStep mStep = ComputePassStep::PreDraw;
    };

}
