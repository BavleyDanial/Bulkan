<a id="readme-top"></a>

<details>
  <summary>Table of Contents</summary>

1. [About The Project](#about-the-project)
2. [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Installation](#installation)
3. [Usage](#usage)
4. [Roadmap](#roadmap)
5. [Known Issues](#known-issues)
</details>

## About The Project

![example-1-screenshot]
![example-2-screenshot]

A powerful rendering engine, built from the ground-up for performance and modularity.

Current Features:
* ECS System
* Data-Oriented Design
* Lambertian Diffuse
* ImGui UI
* Forward Rendering
* Compute Passes
* Draw Passes
* Skybox support

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Getting Started

### Prerequisites

* CMake 3.13+
* Vulkan SDK & Vulkan 1.3+ (developed and tested on Vulkan SDK 1.4.313.1)
* C++23 compiler

<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Installation

```
git clone "https://github.com/BavleyDanial/Bulkan"
cmake -B build
```

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- USAGE EXAMPLES -->
## Usage

Creating a new custom Application Instance:
```c++
#include <EntryPoint.h> // Should always be first
#include <Engine.h>

class ExampleLayer : public Layer { // You can define however many layers you want to add custom behaviour and use the Engine's update & rendering loops
public:
      virtual void OnAttach() {} // Called from Engine when the layer is first added
      virtual void OnDetach() {} // Called from Engine when the layer is removed
      virtual void OnUpdate(float dt) {} // Called from Engine every frame, the engine will then render anything this layer (or others as well) have submitted for rendering
      virtual void OnUIRender() {} // Called from Engine every frame, the engine will then render any UI this layer (or others as well) have submitted for rendering
};

class ExampleApp : public Application { // This is your actual application, think of it as your main class
public: 
    ExampleApp() { PushLayer(std::make_unique<ExampleLayer>()); }
}

VKRE::Application* VKRE::CreateApplication() { // This must be defined to kickstart the engine
    return new ExampleApp();
}

```

Creating a new Entity:

```c++
    virtual void OnAttach() {
        mScene = std::make_unique<Scene>();
        mCamera = mScene->AddCamera("camera");
        mSceneRenderer.SetScene(mScene.get());
        mSceneRenderer.SetCamera(mCamera);

        const MeshAsset* testMesh = AssetManager::LoadMesh("res/models/testmesh.obj");
        mTest = mScene->AddEntity("Test");
        mTest.Add<StaticMeshComponent>({ testMesh });
        mTest.Add<TransformComponent>({ .Position = glm::vec3(0.0f, 0.0f, 50.0f), .Rotation = glm::vec3(-90.0f, 0.0f, 0.0f) });
        
        // All Entities have TransformComponent by default so you can opt to not do mTest.Add<TransformComponent>
        // But it still is useful if you want to define a default location, rotation, scale as it overrides previous components
    }
    
    virtual void OnUpdate(float td) {
        mScene->OnUpdate(dt);
        mSceneRenderer.Render(); // this submits rendering commands to engine, then engine queues them up and renders them
    }
```

UI Example:
```c++
virtual void OnUIRender() {
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::PopStyleVar();

    if (ImGui::Begin("CamSettings")) {
        ImGui::DragFloat("speed", &mSpeed, 0.1f, 0.0f);
        ImGui::DragFloat("sensitivity", &mSensititvity, 0.1f, 0.0f);
        ImGui::Separator();
    }
    ImGui::End();

    // This is currently the way to get entities of specific transforms, in the future, there will be an API for that
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

```

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- ROADMAP -->
## Roadmap

- [x] Add Textures
- [x] Add Cubemaps/Skyboxes
- [x] Rename Repo and namespaces to Bulkan or something related to that
- [ ] Add Materials - In Progress
- [ ] Add Blinn-Phong Specular Model - In Progress
- [ ] Add PBR lighting - Soon
- [ ] Use Slang instead of GLSL
- [ ] Add Render Graph
  - [ ] Add Deffered Rendering
- [ ] Add a proper Editor

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- Known Issues -->
## Known Issues

* Currently there is no way to change viewport
* All vendors are included in the vendors folder, in the future this will be done either with git submodules or through cmake

<p align="right">(<a href="#readme-top">back to top</a>)</p>

[example-1-screenshot]: github/images/example_image_1_1.png
[example-2-screenshot]: github/images/example_image_2_0.png
