#pragma once

#include <engine/engine.hpp>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>

#include <string>

struct WindowInfo final {
    int width = 1024;
    int height = 1024;
    std::string name = "raytracer";
    bool disableCursor = false;
};

const float SENSITIVITY = 0.001;

class Window final {
public:
    Window(const WindowInfo& info);
    Window() : Window(WindowInfo{}) {}
    ~Window();

    [[nodiscard]] uint32_t getWidth() const noexcept { return width; }
    [[nodiscard]] uint32_t getHeight() const noexcept { return height; }
    [[nodiscard]] glm::vec2 getPos() const noexcept { return delta; }
    [[nodiscard]] glm::vec3 getInput() const noexcept { return input; }
    [[nodiscard]] bool hasMoved() const noexcept { return moved; }

    bool shouldClose() const;
    void pollEvents();

    const VkSurfaceKHR createSurface(const engine::Context& context) const;
    const std::vector<const char*> getRequiredExtensions() const;

    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);

private:
    GLFWwindow* window = nullptr;

    uint32_t width;
    uint32_t height;
    bool closeRequested = false;

    bool initialized = false;
    bool moved = false;
    glm::vec2 cursorPos;
    glm::vec2 delta{ 0.0, 0.0 };

    glm::vec3 input{ 0.0, 0.0, 0.0 };
};
