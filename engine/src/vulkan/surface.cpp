#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace engine::vulkan {

Surface::Surface(const Instance& instance, VkSurfaceKHR surface)
    : surface(vk::raii::SurfaceKHR(instance.get(), surface)) {
}

}
