#pragma once

#include <Vulkan/VulkanUtils.h>
#include <Vulkan/VulkanContext.h>

#include <Vulkan/VulkanFrameManager.h>
#include <Vulkan/VulkanPresenter.h>

#include <Vulkan/VulkanDrawPass.h>
#include <Vulkan/VulkanComputePass.h>
#include <Vulkan/VulkanImGuiPass.h>

#include <Vulkan/VulkanResourceCache.h>
#include <Vulkan/VulkanDescriptors.h>
#include <Vulkan/VulkanImage.h>

#include <Renderer/RHIRenderer.h>
#include <ResourceManager/Resources.h>
#include <Assets/Shader.h>
#include <Core/Events/Events.h>

#include <array>
#include <limits>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

namespace VKRE {

    using ComputePassHandle = uint32_t;
    static constexpr ComputePassHandle INVALID_COMPUTE_PASS = std::numeric_limits<uint32_t>::max();

    using DrawPassHandle = uint32_t;
    static constexpr DrawPassHandle INVALID_DRAW_PASS = std::numeric_limits<uint32_t>::max();

    struct ComputePassDesc {
        const ShaderAsset* Shader = nullptr;
        std::string debugName;
        std::vector<VkPushConstantRange> pushConstantRanges;
        uint32_t workgroupX = 16;
        uint32_t workgroupY = 16;
        uint32_t workgroupZ = 1;
    };

    struct DrawPassDesc {
        const ShaderAsset* Shader = nullptr;
        std::string debugName;
        std::vector<VkPushConstantRange> pushConstantRanges;

        std::vector<VkFormat> colorAttachmentFormats;
        VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED;
        VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

        VkBool32 depthTestEnable = VK_FALSE;
        VkBool32 depthWriteEnable = VK_FALSE;
        VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
        VkCullModeFlags cullMode = VK_CULL_MODE_NONE;

        VkAttachmentLoadOp colorLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        VkAttachmentLoadOp depthLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    };

    class VulkanRenderer {
    public:
        VulkanRenderer(VulkanContext& context, const RHIRendererSettings& settings);
        ~VulkanRenderer();

        void ReSize(const WindowResizeEvent& event);
        glm::vec2 GetViewportDimensions() const;

        void BeginFrame();
        void Render();
        void OnImGui();

        void UploadSceneData(const SceneUBO& sceneData);

        DrawPassHandle AddDrawPass(const DrawPassDesc& desc);
        void SubmitMeshDraw(DrawPassHandle handle, const MeshDrawCommand& cmd);
        void SubmitSkyboxDraw(DrawPassHandle handle, const SkyboxDrawCommand& cmd);

        void ActivateDrawPass(DrawPassHandle handle) { mDrawPasses[handle].SetActive(true); }
        void DeActivateDrawPass(DrawPassHandle handle) { mDrawPasses[handle].SetActive(false); }

        ComputePassHandle AddComputePass(const ComputePassDesc& desc);
        void SetComputePassData(ComputePassHandle handle, const void* data, uint32_t size);
        void ActivateComputePass(ComputePassHandle handle) { mComputePasses[handle].SetActive(true); }
        void DeActivateComputePass(ComputePassHandle handle) { mComputePasses[handle].SetActive(false); }

        void ClearImage(VkCommandBuffer cmd);

        VulkanResourceCache& GetResourceCache() { return *mResourceCache; }

    private:
        void CreateDrawImage();
        void ReCreateDrawImage();

        void InitPasses();
        void InitDescriptors();
        void CreateSceneUniformBuffers();
        void InitDrawImageDescriptor();

    private:
        VulkanContext& mContext;

        std::unique_ptr<VulkanFrameManager> mFrameManager;
        std::unique_ptr<VulkanPresenter> mPresenter;
        std::unique_ptr<VulkanResourceCache> mResourceCache;

        std::unique_ptr<VulkanImageData> mDrawImage; // TODO: Once done with managing deletion/creation internally turn into a value rather than a pointer
        std::unique_ptr<VulkanImageData> mDepthImage;

        std::vector<VulkanDrawPass> mDrawPasses;
        std::vector<VulkanComputePass> mComputePasses;
        std::unique_ptr<VulkanImGuiPass> mImGuiPass;

        /* Global Descriptors */
        VulkanFixedDescriptorAllocator mGlobalAllocator;
        VkDescriptorSet mDrawImageSet;
        VkDescriptorSetLayout mDrawImageLayout;

        /* Scene Descriptors */
        VkDescriptorSet mCurrentSceneSet = VK_NULL_HANDLE;
        VkDescriptorSetLayout mSceneLayout;

        std::vector<VulkanGPUBufferData> mSceneUniformBuffers;
    };

}
