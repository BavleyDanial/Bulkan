#include <Vulkan/VulkanRenderer.h>

#include <Core/Application.h>
#include <Core/Profiler.h>

#include <ImGui/Backend/ImGuiGLFW.h>
#include <ImGui/Backend/ImGuiVulkan.h>

#include <cstddef>
#include <cstring>
#include <glm/glm.hpp>

#include <cassert>
#include <memory>
#include <vulkan/vulkan_core.h>

namespace VKRE {

    VulkanRenderer::VulkanRenderer(VulkanContext& context, const RHIRendererSettings& settings)
    :mContext(context) {
        Application::GetInstance().GetEventDispatcher().RegisterListener<WindowResizeEvent>(this, &VulkanRenderer::ReSize);

        mFrameManager = std::make_unique<VulkanFrameManager>(mContext);
        mPresenter = std::make_unique<VulkanPresenter>(mContext);
        mResourceCache = std::make_unique<VulkanRHI>(mContext, settings, mFrameManager->GetFramesInFlight());

        CreateDrawImage();
        CreateSceneUniformBuffers();
        InitDescriptors();
        InitPasses();
    }

    VulkanRenderer::~VulkanRenderer() {
        vkDeviceWaitIdle(mContext.GetLogicalDevice().handle);

        mDrawPasses.clear();
        mComputePasses.clear();
        mImGuiPass.reset();

        mPresenter.reset();
        mFrameManager.reset();

        ImageUtils::ReleaseImage(mContext, &mDrawImage);
        ImageUtils::ReleaseImage(mContext, &mDepthImage);
        for (auto& buffer : mSceneUniformBuffers)
            GPUBufferUtils::ReleaseBuffer(mContext, &buffer);

        mResourceCache->DestroyAll();
        mResourceCache.reset();

        mGlobalAllocator.DestroyPool(mContext.GetLogicalDevice().handle);

        vkDestroyDescriptorSetLayout(mContext.GetLogicalDevice().handle, mDrawImageLayout, nullptr);
        vkDestroyDescriptorSetLayout(mContext.GetLogicalDevice().handle, mSceneLayout, nullptr);
    }

    void VulkanRenderer::UploadSceneData(const SceneUBO& sceneData) {
        uint32_t frameIndex = mFrameManager->GetTotalFramesCount() % mSceneUniformBuffers.size();
        VulkanGPUBufferData& bufferData = mSceneUniformBuffers[frameIndex];

        memcpy(bufferData.info.pMappedData, &sceneData, sizeof(SceneUBO));
        VkDevice device = mContext.GetLogicalDevice().handle;

        VulkanFrameData& frame = mFrameManager->GetCurrentFrame();
        mCurrentSceneSet = frame.FrameDescriptors.Allocate(device, mSceneLayout, nullptr);

        VulkanDescriptorWriter writer;
        writer.WriteBuffer(0, bufferData.buffer, sizeof(SceneUBO), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        writer.UpdateSet(device, mCurrentSceneSet);
    }

    DrawPassHandle VulkanRenderer::AddDrawPass(const DrawPassDesc& desc) {
        if (!desc.Shader || desc.Shader->VertexSpirV.empty() || desc.Shader->FragmentSpirV.empty()) {
            std::println("VulkanRenderer::AddDrawPass '{}' needs a shader with a vertex and a fragment stage", desc.debugName);
            return INVALID_DRAW_PASS;
        }

        VulkanPipelineLayoutKey layoutKey {
            .descriptorSetLayouts = {
                mDrawImageLayout,
                mSceneLayout,
                mResourceCache->GetBindlessTexture2DLayout(),
                mResourceCache->GetBindlessTextureCubeLayout(),
            },
            .pushConstantRanges = desc.pushConstantRanges
        };

        VkPipelineLayout pipelineLayout = mResourceCache->CreatePipelineLayout(layoutKey);
        if (!pipelineLayout) {
            std::println("VulkanRenderer::AddDrawPass Failed to create or retreive pipeline layout");
            return INVALID_DRAW_PASS;
        }

        VulkanGraphicsShaders shaders{};
        shaders.vertex = { desc.Shader->VertexSpirV, desc.Shader->EntryPoint.c_str() };
        shaders.fragment = { desc.Shader->FragmentSpirV, desc.Shader->EntryPoint.c_str() };

        VulkanGraphicsPipelineKey pipelineKey {};
        pipelineKey.vertexShader = HashShaderStage(shaders.vertex);
        pipelineKey.fragmentShader = HashShaderStage(shaders.fragment);
        pipelineKey.layout = pipelineLayout;
        pipelineKey.colorAttachmentFromats = desc.colorAttachmentFormats;
        pipelineKey.depthAttachmentFormat = desc.depthAttachmentFormat;
        pipelineKey.stencilAttachmentFormat = desc.stencilAttachmentFormat;
        pipelineKey.depthTestEnable = desc.depthTestEnable;
        pipelineKey.depthWriteEnable = desc.depthWriteEnable;
        pipelineKey.depthCompareOp = desc.depthCompareOp;
        pipelineKey.cullMode = desc.cullMode;

        if (!mResourceCache->CreateGraphicsPipeline(pipelineKey, shaders)) {
            std::println("VulkanRenderer::AddDrawPass Failed to create or retreive pipeline");
            return INVALID_DRAW_PASS;
        }

        VkShaderStageFlags pushConstantsShaderStages = 0;
        for (const auto& stage : desc.pushConstantRanges)
            pushConstantsShaderStages |= stage.stageFlags;

        mDrawPasses.emplace_back(mContext.GetLogicalDevice().handle, *mResourceCache, pipelineKey, mDrawImageSet, pushConstantsShaderStages);
        mDrawPasses.back().SetColorLoadOp(desc.colorLoadOp);
        mDrawPasses.back().SetDepthLoadOp(desc.depthLoadOp);
        return static_cast<DrawPassHandle>(mDrawPasses.size() - 1);
    }

    void VulkanRenderer::SubmitMeshDraw(DrawPassHandle handle, const MeshDrawCommand& cmd) {
        if (handle == INVALID_DRAW_PASS || handle >= mDrawPasses.size()) {
            std::println("VulkanRenderer::SubmitMeshDraw Invalid draw pass handle");
            return;
        }
        mDrawPasses[handle].SubmitMeshDraw(cmd);
    }

    void VulkanRenderer::SubmitSkyboxDraw(DrawPassHandle handle, const SkyboxDrawCommand& cmd) {
        if (handle == INVALID_DRAW_PASS || handle >= mDrawPasses.size()) {
            std::println("VulkanRenderer::SubmitSkyboxDraw Invalid draw pass handle");
            return;
        }
        mDrawPasses[handle].SubmitSkyboxDraw(cmd);
    }

    ComputePassHandle VulkanRenderer::AddComputePass(const ComputePassDesc& desc) {
        if (!desc.Shader || desc.Shader->ComputeSpirV.empty()) {
            std::println("VulkanRenderer::AddComputePass '{}' needs a shader with a compute stage", desc.debugName);
            return INVALID_COMPUTE_PASS;
        }

        VulkanPipelineLayoutKey layoutKey {
            .descriptorSetLayouts = { mDrawImageLayout },
            .pushConstantRanges = desc.pushConstantRanges
        };

        VkPipelineLayout pipelineLayout = mResourceCache->CreatePipelineLayout(layoutKey);
        if (!pipelineLayout) {
            std::println("VulkanRenderer::AddComputePass Failed to create or retreive pipeline layout");
            return INVALID_COMPUTE_PASS;
        }

        VulkanShaderStage shader = { desc.Shader->ComputeSpirV, desc.Shader->EntryPoint.c_str() };
        VulkanComputePipelineKey pipelineKey { HashShaderStage(shader), pipelineLayout };
        if (!mResourceCache->CreateComputePipeline(pipelineKey, shader)) {
            std::println("VulkanRenderer::AddComputePass Failed to create or retreive pipeline");
            return INVALID_COMPUTE_PASS;
        }

        mComputePasses.emplace_back(*mResourceCache, pipelineKey, mDrawImageSet, glm::vec3(desc.workgroupX, desc.workgroupY, desc.workgroupZ), desc.Step);
        return static_cast<ComputePassHandle>(mComputePasses.size() - 1);
    }

    void VulkanRenderer::SetComputePassData(ComputePassHandle handle, const void* data, uint32_t size) {
        if (handle == INVALID_COMPUTE_PASS || handle >= mComputePasses.size()) {
            std::println("VulkanRenderer::SetComputePassData Invalid compute pass handle");
            return;
        }
        mComputePasses[handle].SetPushConstantData(data, size);
    }

    void VulkanRenderer::CreateDrawImage() {
        auto [width, height] = mPresenter->GetSwapChain().extent;
        VkExtent3D drawImageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
        VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
        VkImageUsageFlags drawImageUsages{};
        drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
        drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        VmaAllocationCreateInfo drawImageAllocInfo = {};
        drawImageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        drawImageAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        mDrawImage = ImageUtils::ReCreateImage(mContext, &mDrawImage, format, drawImageUsages, drawImageExtent, VK_IMAGE_ASPECT_COLOR_BIT, 1, drawImageAllocInfo);
        mDepthImage = ImageUtils::ReCreateImage(mContext, &mDepthImage, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, drawImageExtent, VK_IMAGE_ASPECT_DEPTH_BIT, 1, drawImageAllocInfo);
    }

    void VulkanRenderer::ReCreateDrawImage() {
        mGlobalAllocator.ClearPools(mContext.GetLogicalDevice().handle);
        CreateDrawImage();
        InitDrawImageDescriptor();

        for (auto& pass : mComputePasses)
            pass.ReBuild(mDrawImageSet);

        for (auto& pass : mDrawPasses)
            pass.ReBuild(mDrawImageSet);
    }

    void VulkanRenderer::InitPasses() {
        mImGuiPass = std::make_unique<VulkanImGuiPass>(mContext, *mPresenter);
    }

    void VulkanRenderer::InitDescriptors() {
        std::vector<VulkanPoolSizeRatio> poolSizes = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
        };

        VkDevice device = mContext.GetLogicalDevice().handle;
        mGlobalAllocator.InitPool(device, 10, poolSizes);

        {   // Compute Descriptor Set 0
            VulkanDescriptorLayoutBuilder layoutBuilder;
            layoutBuilder.AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0);
            mDrawImageLayout = layoutBuilder.Build(device, VK_SHADER_STAGE_COMPUTE_BIT);
        }

        {   // Per-Scene Uniform Buffers Descriptor Set 1
            VulkanDescriptorLayoutBuilder layoutBuilder;
            layoutBuilder.AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0);
            mSceneLayout = layoutBuilder.Build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        }

        InitDrawImageDescriptor();
    }

    void VulkanRenderer::InitDrawImageDescriptor() {
        mDrawImageSet = mGlobalAllocator.Allocate(mContext.GetLogicalDevice().handle, mDrawImageLayout);

        VulkanDescriptorWriter writer;
        writer.WriteImage(0, mDrawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        writer.UpdateSet(mContext.GetLogicalDevice().handle, mDrawImageSet);
    }

    void VulkanRenderer::CreateSceneUniformBuffers() {
        uint32_t framesInFlight = mFrameManager->GetFramesInFlight();
        mSceneUniformBuffers.reserve(framesInFlight);

        for (uint32_t i = 0; i < framesInFlight; i++) {
            VmaAllocationCreateInfo allocInfo{};
            allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

            VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            mSceneUniformBuffers.emplace_back(GPUBufferUtils::CreateBuffer(mContext, sizeof(SceneUBO), usage, allocInfo));
        }

    }

    void VulkanRenderer::BeginFrame() {
        VKRE_SCOPED_TIMER("VulkanRenderer::BeginFrame");
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        VulkanFrameData& frame = mFrameManager->GetCurrentFrame();
        VK_CHECK(vkWaitForFences(mContext.GetLogicalDevice().handle, 1, &frame.WaitFence, true, UINT64_MAX));
        mFrameManager->ClearFramePools();

        mResourceCache->BeginFrame(mFrameManager->GetTotalFramesCount());
    }

    void VulkanRenderer::Render() {
        VKRE_SCOPED_TIMER("VulkanRenderer::Render");
        VulkanFrameData& frame = mFrameManager->GetCurrentFrame();

        uint32_t swapchainImageIndex = 0;
        VkResult acquireResult = vkAcquireNextImageKHR(mContext.GetLogicalDevice().handle, mPresenter->GetSwapChain().handle, UINT64_MAX, frame.PresentCompleteSemaphore, nullptr, &swapchainImageIndex);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
            auto [width, height] = Application::GetInstance().GetWindow().GetFrameBufferExtents();
            ReSize({ static_cast<uint32_t>(width), static_cast<uint32_t>(height) });
            return;
        }

        VK_CHECK(acquireResult);
        VK_CHECK(vkResetFences(mContext.GetLogicalDevice().handle, 1, &frame.WaitFence));

        // NOTE: The following is temporary!
        VkCommandBuffer cmd = frame.CommandBuffer;
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo cmdBufferBeginInfo{};
        cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBufferBeginInfo.pNext = nullptr;
        cmdBufferBeginInfo.pInheritanceInfo = nullptr;
        cmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBufferBeginInfo));

        VkExtent2D drawImageExtent = {
            mDrawImage.extent.width,
            mDrawImage.extent.height
        };

        ImageUtils::TransitionImage(cmd, mDrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        // TODO: Replace this with passes
        ClearImage(cmd);

        for (auto& pass : mComputePasses) {
            if (pass.IsActive() && pass.GetActiveStep() == ComputePassStep::PreDraw)
                pass.Execute(cmd, drawImageExtent);
        }

        ImageUtils::TransitionImage(cmd, mDrawImage.image,VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkClearValue clearValue{};
        clearValue.color = {0.0f, 0.0f, 0.0f, 1.0f};
        VkClearValue clearDepthValue{};
        clearDepthValue.depthStencil = {1.0, 0};

        ImageUtils::TransitionImage(cmd, mDepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        VkDescriptorSet bindlessTexture2DSet = mResourceCache->GetBindlessTexture2DSet();
        VkDescriptorSet bindlessTextureCubeSet = mResourceCache->GetBindlessTextureCubeSet();

        for (auto& pass : mDrawPasses) {
            if (!pass.IsActive())
                continue;

            VkRenderingAttachmentInfo colorAttachment = VulkanUtils::AttatchmentInfo(
                mDrawImage.imageView,
                pass.GetColorLoadOp() == VK_ATTACHMENT_LOAD_OP_CLEAR ? &clearValue : nullptr,
                VK_ATTACHMENT_STORE_OP_STORE,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );

            // TODO: Change this to use the same function as above
            VkRenderingAttachmentInfo depthAttachment{};
            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.imageView = mDepthImage.imageView;
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = pass.GetDepthLoadOp();
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachment.clearValue.depthStencil = clearDepthValue.depthStencil;

            RenderTargetInfo targetInfo{};
            targetInfo.colorAttachments = { colorAttachment };
            targetInfo.depthAttachment = &depthAttachment;

            pass.Execute(cmd, drawImageExtent, targetInfo,
                        mCurrentSceneSet, bindlessTexture2DSet, bindlessTextureCubeSet);
        }

        ImageUtils::TransitionImage(cmd, mDrawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
        for (auto& pass : mComputePasses) {
            if (pass.IsActive() && pass.GetActiveStep() == ComputePassStep::PostProcess)
                pass.Execute(cmd, drawImageExtent);
        }
        ImageUtils::TransitionImage(cmd, mDrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        VkImage swapChainImage = mPresenter->GetImages()[swapchainImageIndex];
        ImageUtils::TransitionImage(cmd, swapChainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        ImageUtils::CopyImage(cmd, mDrawImage.image, swapChainImage, drawImageExtent, mPresenter->GetSwapChain().extent);
        ImageUtils::TransitionImage(cmd, swapChainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        mImGuiPass->Execute(cmd, { swapchainImageIndex, nullptr} );

        ImageUtils::TransitionImage(cmd, swapChainImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        VK_CHECK(vkEndCommandBuffer(cmd));

        VkSemaphoreSubmitInfo presentCompleteSemaphoreSubmitInfo{};
        presentCompleteSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        presentCompleteSemaphoreSubmitInfo.semaphore = frame.PresentCompleteSemaphore;
        presentCompleteSemaphoreSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

        VkSemaphoreSubmitInfo renderCompleteSemaphoreSubmitInfo{};
        renderCompleteSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        renderCompleteSemaphoreSubmitInfo.semaphore = mPresenter->GetRenderCompleteSemaphore(swapchainImageIndex);
        renderCompleteSemaphoreSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

        VkCommandBufferSubmitInfo cmdSubmitInfo;
        cmdSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        cmdSubmitInfo.pNext = nullptr;
        cmdSubmitInfo.commandBuffer = cmd;
        cmdSubmitInfo.deviceMask = 0;

        VkSubmitInfo2 info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        info.pNext = nullptr;
        info.waitSemaphoreInfoCount = 1;
        info.pWaitSemaphoreInfos = &presentCompleteSemaphoreSubmitInfo;
        info.signalSemaphoreInfoCount = 1;
        info.pSignalSemaphoreInfos = &renderCompleteSemaphoreSubmitInfo;
        info.commandBufferInfoCount = 1;
        info.pCommandBufferInfos = &cmdSubmitInfo;

        VK_CHECK(vkQueueSubmit2(mContext.GetGraphicsQueue(), 1, &info, frame.WaitFence));
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.pSwapchains = &mPresenter->GetSwapChain().handle;
        presentInfo.swapchainCount = 1;
        presentInfo.pWaitSemaphores = &mPresenter->GetRenderCompleteSemaphore(swapchainImageIndex);
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pImageIndices = &swapchainImageIndex;
        VK_CHECK(vkQueuePresentKHR(mContext.GetGraphicsQueue(), &presentInfo));

        mFrameManager->AdvanceFrame();
    }

    void VulkanRenderer::OnImGui() {}

    void VulkanRenderer::ReSize(const WindowResizeEvent& event) {
        vkDeviceWaitIdle(mContext.GetLogicalDevice().handle);
        mPresenter->ResizeSwapChain(event.Width, event.Height);
        ReCreateDrawImage();
    }

    glm::vec2 VulkanRenderer::GetViewportDimensions() const {
        return {
            mDrawImage.extent.width,
            mDrawImage.extent.height
        };
    }

    void VulkanRenderer::ClearImage(VkCommandBuffer cmd) {
        VkClearColorValue clearValue;
        clearValue = { { 0.0f, 0.0f, 0.0f, 1.0f } };
        VkImageSubresourceRange clearRange = ImageUtils::ImageSubSourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        vkCmdClearColorImage(cmd, mDrawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
    }

}
