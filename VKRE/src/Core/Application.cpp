#include <Core/Application.h>
#include <Core/Profiler.h>

#include <Renderer/Renderer.h>

#include <chrono>
#include <imgui.h>

namespace VKRE {

    Application::Application() {
        if (mInstance) {
            assert("Application has already been initialised!");
        }

        mInstance = this;

        mWindow = std::make_unique<Window>(WindowSpecs{ .resizable = true });
        mResourceManager = std::make_unique<ResourceManager>();
        mAssetManager = std::make_unique<AssetManager>(*mResourceManager);
        mVulkanContext = std::make_unique<VulkanContext>(*mWindow);
        mVulkanRenderer = std::make_unique<VulkanRenderer>(*mVulkanContext, *mResourceManager);

        Renderer::SetRenderer(mVulkanRenderer.get());
    }

    Application::~Application() {
        mVulkanRenderer.reset();
        mVulkanContext.reset();
        mAssetManager.reset();
        mResourceManager.reset();
        mWindow.reset();
    }

    void Application::Run() {
        auto lastFrameTime = std::chrono::high_resolution_clock::now();

        // TODO: Change this to close when the engine decides to close, not when ONE WINDOW decides it's done. This will help with multiple windows as well.
        while (!mWindow->ShouldClose()) {
            Profiler::BeginFrame();

            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - lastFrameTime).count();
            lastFrameTime = now;

            mWindow->OnUpdate();
            mVulkanRenderer->BeginFrame();

            {
                VKRE_SCOPED_TIMER("Application::LayersStack::OnUpdate");
                for (auto& layer : mLayersStack)
                    layer->OnUpdate(dt);
            }
            {
                VKRE_SCOPED_TIMER("Application::LayersStack::OnUIRender");
                for (auto& layer : mLayersStack)
                    layer->OnUIRender();
            }
            {
                VKRE_SCOPED_TIMER("ImGui::Render");
                mVulkanRenderer->OnImGui();
                ImGui::Render();
            }

            mVulkanRenderer->Render();
            Profiler::EndFrame();
        }
    }

}
