#pragma once

#include <Scene/Scene.h>

#include <Renderer/Renderer.h>
#include <Assets/Texture.h>
#include <ResourceManager/ResourceHandles.h>

namespace VKRE {

    class SceneRenderer {
    public:
        SceneRenderer();

        void SetCamera(Entity camera) { mCamera = camera; }
        Entity GetCamera() const { return mCamera; }

        void SetScene(const Scene* scene) { mScene = scene; }
        const Scene* GetScene() const { return mScene; }

        void SetSkybox(ResourceHandle<TextureCubeAsset> cubemap) { mSkybox = cubemap; }
        ResourceHandle<TextureCubeAsset> GetSkybox() const { return mSkybox; }

        void Render();
        uint32_t GetDrawCalls() const { return mDrawCalls; } // TODO: Make a statistics panel that can be turned on and off

    private:
        void RenderScene();
        void RenderSkybox();
        void ApplyPostProcessing();

    private:
        Entity mCamera;
        const Scene* mScene = nullptr;
        ResourceHandle<TextureCubeAsset> mSkybox;

        glm::mat4 mCachedViewMat = glm::mat4(1.0f);
        glm::mat4 mCachedProjMat = glm::mat4(1.0f);

        DrawPassHandle mDrawPass = INVALID_DRAW_PASS;
        DrawPassHandle mSkyboxPass = INVALID_DRAW_PASS;
        ComputePassHandle mPostProcessPass = INVALID_COMPUTE_PASS;

        PostProcessConstants mPostProcessConstants;

        uint32_t mDrawCalls = 0;
    };

}
