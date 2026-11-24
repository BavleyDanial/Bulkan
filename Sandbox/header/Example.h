#pragma once

#include <Engine.h>

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace VKRE;

class ExampleLayer : public Layer {
public:
    ExampleLayer()
        :Layer("Example Layer") {}

    virtual void OnAttach();
    virtual void OnDetach();
    virtual void OnUpdate(float dt);
    virtual void OnUIRender();

private:
    std::unique_ptr<Scene> mScene;
    SceneRenderer mSceneRenderer;
    int32_t mFPS;

    Entity mCamera;
    Entity mTeapot;
    Entity mSun;

    // This layer owns one reference to each; released in OnDetach.
    ResourceHandle<MeshAsset> mTeapotMesh;
    ResourceHandle<TextureCubeAsset> mSkybox;

    float mSpeed = 40.0f;
    float mSensititvity = 60.0f;
    float mLastMouseX = 0;
    float mLastMouseY = 0;
    bool mJustLocked = false;
};
