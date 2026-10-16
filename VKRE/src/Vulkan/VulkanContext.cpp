#include <Vulkan/VulkanContext.h>

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include <Engine.h>
#include <GLFW/glfw3.h>

namespace VKRE {

    VulkanContext::VulkanContext(Window& window) {
        if (mEnableValidationLayers && !VulkanUtils::CheckValidationLayerSupport(mValidationLayers)) {
            std::println("Failed to create Vulkan Instance: Validation Layers are not supported!");
            abort();
        }

        if (!sInstance) {
            VkApplicationInfo appInfo{};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = "VK Rendering Engine";
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0 , 0);
            appInfo.pEngineName = "No Engine";
            appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0);

            // TODO: Check if all required extensions are available
            std::vector<const char*> extensions = window.GetWindowExtensions();
            extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &appInfo;
            createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();
            if (mEnableValidationLayers) {
                createInfo.enabledLayerCount = static_cast<uint32_t>(mValidationLayers.size());
                createInfo.ppEnabledLayerNames = mValidationLayers.data();
            } else {
                createInfo.enabledLayerCount = 0;
                createInfo.ppEnabledLayerNames = nullptr;
            }

            VK_CHECK(vkCreateInstance(&createInfo, nullptr, &sInstance));
        }

        GLFWwindow* glfwWindow = window.GetGLFWwindow();
        if (glfwCreateWindowSurface(sInstance, glfwWindow, nullptr, &mSurface) != VK_SUCCESS) {
            std::println("Failed to create Vulkan Surface!");
            abort();
        }

        VkPhysicalDeviceVulkan12Features features12 = {
            .descriptorIndexing = true,
            .shaderUniformBufferArrayNonUniformIndexing = true,
            .shaderSampledImageArrayNonUniformIndexing = true,
            .shaderStorageBufferArrayNonUniformIndexing = true,
            .descriptorBindingUniformBufferUpdateAfterBind = true,
            .descriptorBindingSampledImageUpdateAfterBind = true,
            .descriptorBindingStorageBufferUpdateAfterBind = true,
            .descriptorBindingPartiallyBound = true,
            .runtimeDescriptorArray = true,
            .bufferDeviceAddress = true,
        };

        VkPhysicalDeviceVulkan13Features features13 = {
            .shaderDemoteToHelperInvocation = true,
            .synchronization2 = true,
            .dynamicRendering = true,
        };

        VulkanPhysicalDeviceSelector deviceSelector(sInstance, mSurface);
        deviceSelector = deviceSelector.SetName("Main Rendering Device")
                            .SetRequiredQueueFamilies({ QueueCapability::Graphics, QueueCapability::Present, QueueCapability::Compute })
                            .SetRequiredExtensions({ VK_KHR_SWAPCHAIN_EXTENSION_NAME })
                            .SetRequiredFeatures({ .shaderInt64 = true })
                            .SetRequiredFeatures12(features12)
                            .SetRequiredFeatures13(features13);


#ifdef PLATFORM_MACOS
        deviceSelector.SetRequiredExtensions({ "VK_KHR_portability_subset" });
#endif

        std::optional<VulkanPhysicalDevice> physicalDevice = deviceSelector.Select();
        if (physicalDevice.has_value()) {
            mPhysicalDevice = physicalDevice.value();
        } else {
            std::println("Failed to choose Vulkan Physical Device!");
            abort();
        }

        VulkanLogicalDeviceBuilder deviceBuilder(mPhysicalDevice);
        std::optional<VulkanLogicalDevice> logicalDevice = deviceBuilder.Build();
        if (logicalDevice.has_value()) {
            mLogicalDevice = logicalDevice.value();
        } else {
            std::println("Failed to choose Vulkan Physical Device!");
            abort();
        }

        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.instance = sInstance;
        allocatorInfo.device = logicalDevice->handle;
        allocatorInfo.physicalDevice = physicalDevice->handle;
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        vmaCreateAllocator(&allocatorInfo, &mAllocator);
    }

    VulkanContext::~VulkanContext() {
        vmaDestroyAllocator(mAllocator);
        mLogicalDevice.Destroy();
        vkDestroySurfaceKHR(sInstance, mSurface, nullptr);
        vkDestroyInstance(sInstance, nullptr);
    }
}
