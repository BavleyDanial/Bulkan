#include <EntryPoint.h>

#include <Sandbox.h>
#include <Example.h>

class Sandbox : public Application {
public:
    Sandbox(bool useExample = true) {
        if (useExample)
            PushLayer(std::make_unique<ExampleLayer>());
        else
            PushLayer(std::make_unique<SandboxLayer>());
    }
};

VKRE::Application* VKRE::CreateApplication() {
    return new Sandbox(true);
}
