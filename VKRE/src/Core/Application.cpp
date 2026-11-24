#include <Core/Application.h>
#include <Core/Profiler.h>

#include <Renderer/Renderer.h>

#include <cassert>
#include <chrono>
#include <imgui.h>

namespace VKRE {

    Application::Application() {
        assert(!mInstance && "Application has already been initialised!");
        mInstance = this;

        // TODO: Read these from a config file (editor builds want bigger pools than the runtime)
        RHIRendererSettings rhiSettings{};
        AssetManagerSettings assetSettings{};

        assert(assetSettings.MaxMeshes * 2 <= rhiSettings.MaxBuffers && "AssetSettings.MaxMeshes must be less than RHISettings.MaxBuffers");
        assert(assetSettings.MaxTextures2D < rhiSettings.MaxTextures2D && "AssetSettings.MaxTextures2D must be less than RHISettings.MaxTexures2D");
        assert(assetSettings.MaxTexturesCube < rhiSettings.MaxTexturesCube && "AssetSettings.MaxTexturesCube must be less than RHISettings.MaxTexuresCube");

        mWindow = std::make_unique<Window>(WindowSpecs{ .resizable = true });
        mVulkanContext = std::make_unique<VulkanContext>(*mWindow);
        mVulkanRenderer = std::make_unique<VulkanRenderer>(*mVulkanContext, rhiSettings);
        mRHIRenderer = std::make_unique<RHIRenderer>(mVulkanRenderer->GetResourceCache());
        mAssetManager = std::make_unique<AssetManager>(assetSettings, *mRHIRenderer);

        Renderer::SetRenderer(mVulkanRenderer.get());
    }

    Application::~Application() {
        mLayersStack.Clear();

        mAssetManager.reset();
        mRHIRenderer.reset();

        mVulkanRenderer.reset();
        mVulkanContext.reset();
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

            mAssetManager->FreePendingUnloads();

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
            if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }

            Profiler::EndFrame();
        }
    }

}
