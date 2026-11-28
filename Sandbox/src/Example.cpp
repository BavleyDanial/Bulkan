#include "Assets/Texture.h"
#include <Example.h>

void ExampleLayer::OnAttach() {
    mScene = std::make_unique<Scene>();
    mCamera = mScene->AddCamera("camera");
    mSceneRenderer.SetScene(mScene.get());
    mSceneRenderer.SetCamera(mCamera);

    mTeapotMesh = AssetManager::LoadMesh("res/models/teapot.obj");
    mTeapot = mScene->AddEntity("Teapot");
    mTeapot.Add<StaticMeshComponent>({ mTeapotMesh });
    mTeapot.Add<TransformComponent>({ .Position = glm::vec3(0.0f, 0.0f, 50.0f), .Rotation = glm::vec3(90.0f, 0.0f, 0.0f) });

    mSun = mScene->AddEntity("Sun");
    mSun.Add<DirectionalLightComponent>({});

    mSkybox = AssetManager::LoadTextureCube("res/textures/Cubemap_Sky_23-512x512.png");
    mSceneRenderer.SetSkybox(mSkybox);
}

void ExampleLayer::OnDetach() {
    AssetManager::Release(mTeapotMesh);
    AssetManager::Release(mSkybox);
}

void ExampleLayer::OnUpdate(float dt) {
    mFPS = 1 / dt;

    TransformComponent& transform = mCamera.GetMutable<TransformComponent>();

    if (Input::KeyPressed(GLFW_KEY_F))
        Input::LockMouse();

    if (Input::KeyPressed(GLFW_KEY_ESCAPE))
        Input::UnLockMouse();

    if (Input::IsMouseLocked()) {
        auto [mouseX, mouseY] = Input::GetMousePosition();
        if (!mJustLocked) {
            float dx = (mouseX - mLastMouseX) * mSensititvity * dt;
            float dy = (mouseY - mLastMouseY) * mSensititvity * dt;
            transform.Rotation.y += dx;
            transform.Rotation.x += dy;

            transform.Rotation.x = glm::clamp(transform.Rotation.x, -89.0f, 89.0f);
        }

        mLastMouseX = mouseX;
        mLastMouseY = mouseY;
        mJustLocked = false;
    } else {
        mJustLocked = true;
    }

    glm::quat rotation = glm::quat(glm::radians(transform.Rotation));
    glm::vec3 forward = rotation * glm::vec3(0, 0, 1);
    glm::vec3 right = rotation * glm::vec3(1, 0, 0);
    glm::vec3 up = rotation * glm::vec3(0, 1, 0);

    if (Input::KeyHeld(GLFW_KEY_W)) {
        transform.Position += forward * mSpeed * dt;
    }

    if (Input::KeyHeld(GLFW_KEY_S)) {
        transform.Position -= forward * mSpeed * dt;
    }

    if (Input::KeyHeld(GLFW_KEY_A)) {
        transform.Position -= right * mSpeed * dt;
    }

    if (Input::KeyHeld(GLFW_KEY_D)) {
        transform.Position += right * mSpeed * dt;
    }

    if (Input::KeyHeld(GLFW_KEY_E)) {
        transform.Position.y += mSpeed * dt;
    }

    if (Input::KeyHeld(GLFW_KEY_Q)) {
        transform.Position.y -= mSpeed * dt;
    }

    mScene->OnUpdate(dt);
    mSceneRenderer.Render();
}

void ExampleLayer::OnUIRender() {
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::PopStyleVar();

    if (ImGui::Begin("Statistics")) {
        ImGui::Text("FPS: %i", mFPS);
    }
    ImGui::End();

    if (ImGui::Begin("Cam Settings")) {
        ImGui::DragFloat("speed", &mSpeed, 0.1f, 0.0f);
        ImGui::DragFloat("sensitivity", &mSensititvity, 0.1f, 0.0f);
        ImGui::Separator();
    }
    ImGui::End();

    if (ImGui::Begin("Sun Settings")) {
        DirectionalLightComponent& light = mSun.GetMutable<DirectionalLightComponent>();
        ImGui::DragFloat3("Direction", glm::value_ptr(light.Direction), 0.1f);
        ImGui::ColorEdit3("Color", glm::value_ptr(light.Color));
        ImGui::DragFloat("Intensity", &light.Intensity, 0.1f, 0.0f, 10.0f);
        ImGui::Separator();
    }
    ImGui::End();

    mScene->GetFlecsWorld().each([&](flecs::entity e, TransformComponent& transform) {
        if (ImGui::Begin(e.name())) {
            ImGui::PushID(static_cast<uint32_t>(e.id()));
            ImGui::DragFloat3("Position", glm::value_ptr(transform.Position), 0.1f);
            ImGui::DragFloat3("Rotation", glm::value_ptr(transform.Rotation), 0.1f);
            ImGui::DragFloat3("Scale", glm::value_ptr(transform.Scale), 0.1f);
            ImGui::PopID();
        }
        ImGui::End();
    });
}
