#include "window.hpp"

Window::Window(const WindowInfo& info) : width(static_cast<uint32_t>(info.width)), height(static_cast<uint32_t>(info.height)) {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(info.width, info.height, info.name.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, keyCallback);

    glfwSetCursorPosCallback(window, cursorPositionCallback);

    if (info.disableCursor) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
}

Window::~Window() {
    glfwDestroyWindow(window);
    glfwTerminate();
}

bool Window::shouldClose() const {
    return closeRequested || glfwWindowShouldClose(window);
}

void Window::pollEvents() {
    if (input.x != 0.0 || input.y != 0.0 || input.z != 0.0) {
        moved = true;
    } else {
        moved = false;
    }
    glfwPollEvents();
}

const VkSurfaceKHR Window::createSurface(const engine::Context& context) const {
    VkSurfaceKHR surface;

    auto result = glfwCreateWindowSurface(*context.getInstance().get(), window, nullptr, &surface);
    if (result != 0) {
        throw std::runtime_error("Failed to create window surface.");
    }

    return surface;
}

const std::vector<const char*> Window::getRequiredExtensions() const {
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    return std::vector(glfwExtensions, glfwExtensions + glfwExtensionCount);
}

void Window::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    Window* wp = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        wp->closeRequested = true;
    }

    float delta = (action == GLFW_PRESS) ? 1.0f :
              (action == GLFW_RELEASE) ? -1.0f : 0.0f;

    if (delta != 0.0f) {
        switch (key) {
            case GLFW_KEY_W:           wp->input.z += delta; break;
            case GLFW_KEY_S:           wp->input.z -= delta; break;
            case GLFW_KEY_A:           wp->input.x -= delta; break;
            case GLFW_KEY_D:           wp->input.x += delta; break;
            case GLFW_KEY_SPACE:       wp->input.y += delta; break;
            case GLFW_KEY_LEFT_CONTROL:wp->input.y -= delta; break;
        }
    }
}

void Window::cursorPositionCallback(GLFWwindow *window, double xpos, double ypos) {
    Window* wp = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    wp->moved= true;
    glm::vec2 pos(static_cast<float>(xpos), static_cast<float>(ypos));

    if (!wp->initialized) {
        wp->initialized = true;
        wp->cursorPos = pos;
        return;
    }

    glm::vec2 delta = (pos - wp->cursorPos) * SENSITIVITY;
    wp->cursorPos = pos;
    wp->delta += delta;
}
