#include <Renderer/RendererCommands.h>
#include <Renderer/SceneRenderer.h>
#include <Renderer/Renderer.h>

#include <Scene/Components.h>
#include <ResourceManager/Resources.h>
#include <AssetsManagers/AssetManager.h>
#include <Core/Profiler.h>

#include <glm/glm.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <imgui.h>

#include <print>
#include <vulkan/vulkan_core.h>

namespace VKRE {

    SceneRenderer::SceneRenderer() {
        ResourceHandle<ShaderAsset> basicShader = AssetManager::LoadShader("res/shaders/basic.glsl");
        ResourceHandle<ShaderAsset> skyboxLDRShader = AssetManager::LoadShader("res/shaders/skybox.glsl");
        ResourceHandle<ShaderAsset> skyboxHDRShader = AssetManager::LoadShader("res/shaders/skybox_hdr.glsl");

        mDrawPass = Renderer::AddDrawPass({
            .Shader = AssetManager::Get(basicShader),
            .debugName = "Scene Draw Pass",
            .pushConstantRanges = { { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DrawPushConstants)} },
            .colorAttachmentFormats = { VK_FORMAT_R16G16B16A16_SFLOAT },
            .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
            .depthTestEnable = true,
            .depthWriteEnable = true,
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .cullMode = VK_CULL_MODE_BACK_BIT,
        });

        mSkyboxLDRPass = Renderer::AddDrawPass({
            .Shader = AssetManager::Get(skyboxLDRShader),
            .debugName = "Skybox LDR Pass",
            .pushConstantRanges = { { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DrawPushConstants)} },
            .colorAttachmentFormats = { VK_FORMAT_R16G16B16A16_SFLOAT },
            .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
            .depthTestEnable = true,
            .depthWriteEnable = false,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
            .cullMode = VK_CULL_MODE_NONE,
            .colorLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .depthLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        });

        mSkyboxHDRPass = Renderer::AddDrawPass({
            .Shader = AssetManager::Get(skyboxHDRShader),
            .debugName = "Skybox HDR Pass",
            .pushConstantRanges = { { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DrawPushConstants)} },
            .colorAttachmentFormats = { VK_FORMAT_R16G16B16A16_SFLOAT },
            .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
            .depthTestEnable = true,
            .depthWriteEnable = false,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
            .cullMode = VK_CULL_MODE_NONE,
            .colorLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .depthLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        });

        // TODO: Keep them (and rebuild the passes) once shaders hot reload.
        AssetManager::Release(basicShader);
        AssetManager::Release(skyboxLDRShader);
        AssetManager::Release(skyboxHDRShader);
    }

    void SceneRenderer::Render() {
        VKRE_SCOPED_TIMER("SceneRenderer::Render");

        if (!mScene) {
            std::println("Couldn't find a scene to render");
            return;
        }

        SceneUBO sceneData{};

        if (mCamera.IsValid()) {
            const TransformComponent& transform = mCamera.Get<TransformComponent>();
            const CameraComponent& camComponent = mCamera.Get<CameraComponent>();

            glm::vec3 position = transform.Position;
            glm::quat rotation = glm::quat(glm::radians(transform.Rotation));
            glm::vec3 forward = rotation * glm::vec3(0, 0, 1);

            glm::vec2 viewport = Renderer::GetViewportDimensions();

            sceneData.View = glm::lookAtLH(position, position + forward, glm::vec3(0, 1, 0));
            sceneData.Projection = glm::perspectiveLH_ZO(glm::radians(camComponent.FOV), viewport.x / viewport.y, camComponent.Near, camComponent.Far);
            sceneData.CameraPosition = glm::vec4(position, 1.0f);
        }

        mScene->GetFlecsWorld().each([&](const DirectionalLightComponent& light) {
            sceneData.LightDirection = glm::vec4(light.Direction, 0.0f);
            sceneData.LightColor = glm::vec4(light.Color, light.Intensity);
        });

        sceneData.ViewPorjection = sceneData.Projection * sceneData.View;
        mCachedViewMat = sceneData.View;
        mCachedProjMat = sceneData.Projection;
        Renderer::UploadSceneData(sceneData);

        mDrawCalls = 0;
        RenderScene();
        RenderSkybox();
    }


    void SceneRenderer::RenderScene() {
        VKRE_SCOPED_TIMER("SceneRenderer::RenderScene");
        mScene->GetFlecsWorld().each([&](const TransformComponent& transform, const StaticMeshComponent& staticMesh) {
            const MeshAsset* mesh = AssetManager::Get(staticMesh.Mesh);
            if (!mesh || mesh->VertexBufferAddress == 0)
                return;

            glm::mat4 worldMatrix = glm::translate(glm::mat4(1.0f), transform.Position)
            * glm::mat4_cast(glm::quat(glm::radians(transform.Rotation)))
            * glm::scale(glm::mat4(1.0f), transform.Scale);

            for (const auto& node : mesh->Nodes) {
                for (uint32_t i = 0; i < node.SubMeshCount; i++) {
                    uint32_t idx = mesh->NodeSubMeshIndices[i + node.SubMeshOffset];
                    const SubMesh& subMesh = mesh->SubMeshes[idx];

                    MeshDrawCommand cmd{};
                    cmd.VertexBufferAddress = mesh->VertexBufferAddress;
                    cmd.IndexBuffer = mesh->IndexBuffer;
                    cmd.IndexCount = subMesh.IndexCount;
                    cmd.BaseIndex = subMesh.BaseIndex;
                    cmd.BaseVertex = subMesh.BaseVertex;
                    cmd.Transform = worldMatrix * node.LocalTransform;

                    if (subMesh.TextureIndex >= 0 && subMesh.TextureIndex < static_cast<int32_t>(mesh->Textures.size()))
                        cmd.TextureIndex = AssetManager::GetTextureIndex(mesh->Textures[subMesh.TextureIndex]);

                    Renderer::SubmitMeshDraw(mDrawPass, cmd);
                    mDrawCalls++;
                }
            }
        });
    }

    void SceneRenderer::RenderSkybox() {
        VKRE_SCOPED_TIMER("SceneRenderer::RenderSkybox");
        const TextureCubeAsset* skybox = AssetManager::Get(mSkybox);
        if (!skybox)
            return;

        glm::mat4 rotation = glm::mat4(glm::mat3(mCachedViewMat));
        SkyboxPushConstants pushConstants;
        pushConstants.ViewProjection = mCachedProjMat * rotation;
        pushConstants.TextureCubeIndex = AssetManager::GetTextureIndex(mSkybox);

        SkyboxDrawCommand cmd;
        cmd.PushConstants = pushConstants;

        // TODO: instead of branching every time we render a skybox, make it so that we remove the drawpass if the new skybox set is different from the current one and add the new drawpass
        if (skybox->Format == TextureFormat::R16G16B16A16_SFLOAT || skybox->Format == TextureFormat::R32G32B32A32_SFLOAT)
            Renderer::SubmitSkyboxDraw(mSkyboxHDRPass, cmd);
        else
            Renderer::SubmitSkyboxDraw(mSkyboxLDRPass, cmd);
    }

}
