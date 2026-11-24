#pragma once

#include <Engine.h>

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace VKRE;

class SandboxLayer : public Layer {
public:
    SandboxLayer()
        :Layer("Sandbox Layer") {}

    virtual void OnAttach();
    virtual void OnDetach();
    virtual void OnUpdate(float dt);
    virtual void OnUIRender();

private:
    // TODO: use my Entity abstraction instead
    void DrawEntityNode(flecs::entity e);

private:
    std::unique_ptr<Scene> mScene;
    SceneRenderer mSceneRenderer;
    int32_t mFPS;
    uint32_t mTrianglesPerMesh;
    uint32_t mTrianglesTotal;

    flecs::entity mSelectedEntity; // TODO: Use my Entity abstraction

    Entity mCamera;
    Entity mSun;

    // This layer owns one reference to each; released in OnDetach.
    ResourceHandle<MeshAsset> mSponzaMesh;
    ResourceHandle<TextureCubeAsset> mSkybox;

    float mSpeed = 40.0f;
    float mSensititvity = 60.0f;
    float mLastMouseX = 0;
    float mLastMouseY = 0;
    bool mJustLocked = false;
};
