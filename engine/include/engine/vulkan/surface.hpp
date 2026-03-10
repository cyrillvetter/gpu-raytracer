#pragma once

#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace engine::vulkan {

class Surface final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(Surface)
    Surface() = delete;

    explicit Surface(const Instance& instance, VkSurfaceKHR surface);

    [[nodiscard]] const vk::raii::SurfaceKHR& get() const noexcept { return surface; }

private:
    vk::raii::SurfaceKHR surface = nullptr;
};

}
