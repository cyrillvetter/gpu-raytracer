#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace engine::vulkan {

Semaphore::Semaphore(const Device& device)
    : semaphore(vk::raii::Semaphore(device.get(), vk::SemaphoreCreateInfo{})) {}

}
