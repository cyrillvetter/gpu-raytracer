#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace engine::vulkan {

CommandPool::CommandPool(const Device& device, const CommandPoolInfo& info)
    : commandPool(device.get(), vk::CommandPoolCreateInfo{
        .flags = info.shortLived
            ? vk::CommandPoolCreateFlagBits::eTransient
            : vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = info.queueFamilyIndex
    }) {}

}
