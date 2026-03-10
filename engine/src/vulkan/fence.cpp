#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <cstdint>

namespace engine::vulkan {

Fence::Fence(const Device& device)
    : fence(vk::raii::Fence(device.get(), vk::FenceCreateInfo{
        .flags = vk::FenceCreateFlagBits::eSignaled
    })) {}

void Fence::reset(const Device& device) const {
    device.get().resetFences(*fence);
}

void Fence::waitIndefinitely(const Device& device) const {
    while (vk::Result::eTimeout == device.get().waitForFences(*fence, vk::True, UINT64_MAX));
}

}
