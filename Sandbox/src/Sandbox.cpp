#include <Sandbox.h>

#include <imgui.h>

void SandboxLayer::OnAttach() {
    mScene = std::make_unique<Scene>();
    mCamera = mScene->AddCamera("camera");
    mSceneRenderer.SetScene(mScene.get());
    mSceneRenderer.SetCamera(mCamera);

    mSponzaMesh = AssetManager::LoadMesh("assets/models/main_sponza/NewSponza_Main_glTF_003.gltf");
    uint32_t indices = 0;
    if (const MeshAsset* sponzaMesh = AssetManager::Get(mSponzaMesh)) {
        for (const auto& submesh : sponzaMesh->SubMeshes)
            indices += submesh.IndexCount;
    }

    uint32_t i = 0;
    for (uint32_t x = 0; x < 5; x++) {
        for (uint32_t z = 0; z < 5; z++) {
            std::string name = "Sponza " + std::to_string(i);
            Entity sponza = mScene->AddEntity(name.c_str());
            sponza.Add<StaticMeshComponent>({ mSponzaMesh }); // 25 entities share one reference
            sponza.Add<TransformComponent>({ .Position = glm::vec3(50.0f * x, 0.0f, 50.0f * z) });
            i++;
        }
    }

    mTrianglesPerMesh = indices / 3;
    mTrianglesTotal = mTrianglesPerMesh * i;

    mSun = mScene->AddEntity("Sun");
    mSun.Add<DirectionalLightComponent>({});
    mSkybox = AssetManager::LoadTextureCube("assets/textures/StandardCubeMap.hdr");
    mSceneRenderer.SetSkybox(mSkybox);
}

void SandboxLayer::OnDetach() {
    AssetManager::Release(mSponzaMesh);
    AssetManager::Release(mSkybox);
}

void SandboxLayer::OnUpdate(float dt) {
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

void SandboxLayer::OnUIRender() {
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::PopStyleVar();

    if (ImGui::Begin("Hierarchy")) {
        auto rootEntities = mScene->GetFlecsWorld().query_builder()
            .without(flecs::ChildOf, flecs::Any)
            .without(flecs::Module)
            .without<flecs::Component>()
            .cache_kind(flecs::QueryCacheNone)
            .build();

        rootEntities.each([&](flecs::entity e) {
            DrawEntityNode(e);
        });
    }
    ImGui::End();

    if (ImGui::Begin("Inspector")) {
        if (mSelectedEntity.is_valid() && mSelectedEntity.is_alive()) {
            ImGui::Text("Name: %s", mSelectedEntity.name().c_str());
            ImGui::Separator();

            if (mSelectedEntity.has<TransformComponent>()) {
                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& transform = mSelectedEntity.get_mut<TransformComponent>();
                    ImGui::DragFloat3("Position", glm::value_ptr(transform.Position), 0.1f);
                    ImGui::DragFloat3("Rotation", glm::value_ptr(transform.Rotation), 0.1f);
                    ImGui::DragFloat3("Scale", glm::value_ptr(transform.Scale), 0.1f);
                }
            }

            if (mSelectedEntity.has<DirectionalLightComponent>()) {
                if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                    DirectionalLightComponent& light = mSun.GetMutable<DirectionalLightComponent>();
                    ImGui::DragFloat3("Direction", glm::value_ptr(light.Direction), 0.1f);
                    ImGui::ColorEdit3("Color", glm::value_ptr(light.Color));
                    ImGui::DragFloat("Intensity", &light.Intensity, 0.1f, 0.0f, 10.0f);
                }
            }
        }
    } else {
        ImGui::TextDisabled("Select an entity from the hierarchy to inspect");
    }
    ImGui::End();

    if (ImGui::Begin("Statistics")) {
        ImGui::Text("FPS: %i", mFPS);
        ImGui::Text("Triangles Per Mesh: %i", mTrianglesPerMesh);
        ImGui::Text("Triangles Total: %i", mTrianglesTotal);
        ImGui::Text("Draw Calls: %i", mSceneRenderer.GetDrawCalls());
    }
    ImGui::End();

    if (ImGui::Begin("Cam Settings")) {
        ImGui::DragFloat("speed", &mSpeed, 0.1f, 0.0f);
        ImGui::DragFloat("sensitivity", &mSensititvity, 0.1f, 0.0f);
        ImGui::Separator();
    }
    ImGui::End();
}

void SandboxLayer::DrawEntityNode(flecs::entity e) {
    auto children = e.world().query_builder()
        .with(flecs::ChildOf, e)
        .cache_kind(flecs::QueryCacheNone)
        .build();

    bool hasChildren = children.count() > 0;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (mSelectedEntity == e)
        flags |= ImGuiTreeNodeFlags_Selected;
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf;

    bool opened = ImGui::TreeNodeEx((void*)(uintptr_t)e.id(), flags, "%s", e.name().c_str());

    if (ImGui::IsItemClicked())
        mSelectedEntity = e;

    if (opened) {
        if (hasChildren) {
            children.each([&](flecs::entity child) {
                DrawEntityNode(child);
            });
        }

        ImGui::TreePop();
    }
}
