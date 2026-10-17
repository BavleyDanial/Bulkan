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

namespace VKRE {

    SceneRenderer::SceneRenderer() {
        const ShaderAsset* basicShader = AssetManager::LoadShader("res/shaders/basic.glsl");
        const ShaderAsset* skyboxLDRShader = AssetManager::LoadShader("res/shaders/skybox.glsl");
        const ShaderAsset* skyboxHDRShader = AssetManager::LoadShader("res/shaders/skybox_hdr.glsl");

        mDrawPass = Renderer::AddDrawPass({
            .VertexShader = basicShader->VertexShader.Get(),
            .FragmentShader = basicShader->FragmentShader.Get(),
            .debugName = "Scene Draw Pass",
            .pushConstantRanges = { { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DrawPushConstants)} },
            .colorAttachmentFormats = { VK_FORMAT_R16G16B16A16_SFLOAT },
            .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
            .depthTestEnable = true,
            .depthWriteEnable = true,
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .cullMode = VK_CULL_MODE_NONE,
        });

        mSkyboxLDRPass = Renderer::AddDrawPass({
            .VertexShader = skyboxLDRShader->VertexShader.Get(),
            .FragmentShader = skyboxLDRShader->FragmentShader.Get(),
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
            .VertexShader = skyboxHDRShader->VertexShader.Get(),
            .FragmentShader = skyboxHDRShader->FragmentShader.Get(),
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
            sceneData.Projection = glm::perspectiveLH_NO(glm::radians(camComponent.FOV), viewport.x / viewport.y, camComponent.Near, camComponent.Far);
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
            if (!staticMesh.Asset)
                return;

            uint64_t vbAddress = Renderer::GetBufferDeviceAddress(staticMesh.Asset->VertexBuffer.Get());
            if (vbAddress == 0) return;

            glm::mat4 worldMatrix = glm::translate(glm::mat4(1.0f), transform.Position)
            * glm::mat4_cast(glm::quat(glm::radians(transform.Rotation)))
            * glm::scale(glm::mat4(1.0f), transform.Scale);

            for (const auto& node : staticMesh.Asset->Nodes) {
                for (uint32_t i = 0; i < node.SubMeshCount; i++) {
                    uint32_t idx = staticMesh.Asset->NodeSubMeshIndices[i + node.SubMeshOffset];
                    const SubMesh& mesh = staticMesh.Asset->SubMeshes[idx];

                    MeshDrawCommand cmd{};
                    cmd.VertexBufferAddress = vbAddress;
                    cmd.IndexBuffer = staticMesh.Asset->IndexBuffer.Get();
                    cmd.IndexCount = mesh.IndexCount;
                    cmd.BaseIndex = mesh.BaseIndex;
                    cmd.BaseVertex = mesh.BaseVertex;
                    cmd.Transform = worldMatrix * node.LocalTransform;

                    if (mesh.TextureIndex >= 0 && mesh.TextureIndex < static_cast<int32_t>(staticMesh.Asset->Textures.size()))
                        cmd.TextureIndex = staticMesh.Asset->TexturesIndices[mesh.TextureIndex];

                    Renderer::SubmitMeshDraw(mDrawPass, cmd);
                    mDrawCalls++;
                }
            }
        });
    }

    void SceneRenderer::RenderSkybox() {
        VKRE_SCOPED_TIMER("SceneRenderer::RenderSkybox");
        if (!mSkybox || mSkybox->BindlessIndex == -1)
            return;

        glm::mat4 rotation = glm::mat4(glm::mat3(mCachedViewMat));
        SkyboxPushConstants pushConstants;
        pushConstants.ViewProjection = mCachedProjMat * rotation;
        pushConstants.TextureCubeIndex = mSkybox->BindlessIndex;

        SkyboxDrawCommand cmd;
        cmd.PushConstants = pushConstants;

        // TODO: instead of branching every time we render a skybox, make it so that we remove the drawpass if the new skybox set is different from the current one and add the new drawpass
        if (mSkybox->Format == TextureFormat::R16G16B16A16_SFLOAT || mSkybox->Format == TextureFormat::R32G32B32A32_SFLOAT)
            Renderer::SubmitSkyboxDraw(mSkyboxHDRPass, cmd);
        else
            Renderer::SubmitSkyboxDraw(mSkyboxLDRPass, cmd);
    }

}
