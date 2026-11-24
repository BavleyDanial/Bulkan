#include <Vulkan/VulkanImGuiPass.h>

#include <ImGui/Backend/ImGuiVulkan.h>
#include <ImGui/Backend/ImGuiGLFW.h>
#include <imgui.h>

#include <Core/Application.h>

namespace VKRE {


    VulkanImGuiPass::VulkanImGuiPass(VulkanContext& context, VulkanPresenter& presenter)
    :mContext(context), mPresenter(presenter) {
        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;

        VK_CHECK(vkCreateDescriptorPool(mContext.GetLogicalDevice().handle, &pool_info, nullptr, &mImGuiPool));

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGuiStyle& style = ImGui::GetStyle();
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        ImGui_ImplGlfw_InitForVulkan(Application::GetInstance().GetWindow().GetGLFWwindow(), true);

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = mContext.GetInstance();
        init_info.PhysicalDevice = mContext.GetPhysicalDevice().handle;
        init_info.Device = mContext.GetLogicalDevice().handle;
        init_info.Queue = mContext.GetGraphicsQueue();
        init_info.DescriptorPool = mImGuiPool;
        init_info.MinImageCount = 3;
        init_info.ImageCount = 3;
        init_info.UseDynamicRendering = true;

        ImGui_ImplVulkan_PipelineInfo pipelineInfo{};
        pipelineInfo.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        pipelineInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        pipelineInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &mPresenter.GetSwapChain().imageFormat.format;
        pipelineInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        init_info.PipelineInfoMain = pipelineInfo;

        ImGui_ImplVulkan_Init(&init_info);
    }

    VulkanImGuiPass::~VulkanImGuiPass() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(mContext.GetLogicalDevice().handle, mImGuiPool, nullptr);
    }

    void VulkanImGuiPass::Execute(VkCommandBuffer cmd, const FrameInfo& frameInfo) {
        VkImageView targetImageView = mPresenter.GetImageViews()[frameInfo.swapchainImageIdx];

        VkRenderingAttachmentInfo colorAttachment = VulkanUtils::AttatchmentInfo(targetImageView, nullptr, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingInfo renderInfo = VulkanUtils::RenderingInfo(mPresenter.GetSwapChain().extent, &colorAttachment, nullptr);

        vkCmdBeginRendering(cmd, &renderInfo);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        vkCmdEndRendering(cmd);
    }


}
