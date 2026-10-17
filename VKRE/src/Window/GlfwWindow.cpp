#include <GLFW/glfw3.h>

#include <Window/GlfwWindow.h>
#include <Core/Application.h>
#include <Core/Profiler.h>

#include <cassert>

namespace VKRE {

    Window::Window(const WindowSpecs& specs)
        :mGLFWwindow(nullptr), mSpecs(specs) {

        if (!glfwInit())
            assert("Couldn't initialise glfw");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        if (mSpecs.resizable)
            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        else
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        mGLFWwindow = glfwCreateWindow(static_cast<int>(mSpecs.width), static_cast<int>(mSpecs.height), mSpecs.title.c_str(), nullptr, nullptr);
        if (!mGLFWwindow) {
            glfwTerminate();
            assert("Couldn't initialise window");
        }

        glfwMakeContextCurrent(mGLFWwindow);

        if (mSpecs.vsync)
            glfwSwapInterval(1);

        if (mSpecs.fullscreen) {
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(mGLFWwindow, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            mSpecs.width = mode->width;
            mSpecs.height = mode->height;
        }

        Input::SetCurrentWindow(mGLFWwindow);

        glfwSetWindowUserPointer(mGLFWwindow, this);
        glfwSetWindowSizeCallback(mGLFWwindow, [](GLFWwindow* window, int width, int height) {
            Window* VKREWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));
            VKREWindow->Resize(width, height);

            WindowResizeEvent event{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
            Application::GetInstance().GetEventDispatcher().BroadcastToListeners(event);
        });
   }

    Window::~Window() {
        glfwDestroyWindow(mGLFWwindow);
        glfwTerminate();
    }

    void Window::Close() const {
        glfwSetWindowShouldClose(mGLFWwindow, true);
    }

    void Window::SetWidth(int width) {
        Resize(width, mSpecs.height);
    }

    void Window::SetHeight(int height) {
        Resize(mSpecs.width, height);
    }

    void Window::Resize(int width, int height) {
        mSpecs.width = width;
        mSpecs.height = height;
        glfwSetWindowSize(mGLFWwindow, static_cast<int>(mSpecs.width), static_cast<int>(mSpecs.height));
    }

    std::pair<int32_t, int32_t> Window::GetFrameBufferExtents() const {
        int32_t width = 0;
        int32_t height = 0;

        glfwGetFramebufferSize(mGLFWwindow, &width, &height);
        return std::make_pair(width, height);
    }

    void Window::OnUpdate() const {
        VKRE_SCOPED_TIMER("Window::OnUpdate");
        glfwSwapBuffers(mGLFWwindow);
        Input::SetCurrentWindow(mGLFWwindow);
        Input::OnUpdate();
    }

    std::vector<const char*>Window::GetWindowExtensions() const {
        uint32_t extensionCount = 0;
        const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
        return std::vector<const char*>(extensions, extensions + extensionCount);
    }

}

