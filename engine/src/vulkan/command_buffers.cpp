#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace engine::vulkan {

CommandBuffers::CommandBuffers(const Device& device, const CommandPool& commandPool, const CommandBuffersInfo& info)
    : commandBuffers(vk::raii::CommandBuffers(device.get(), vk::CommandBufferAllocateInfo{
        .commandPool = commandPool.get(),
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = info.count
    })), count(info.count) {}

const vk::raii::CommandBuffer& CommandBuffers::beginRecording(size_t bufferIndex) const {
    const auto& cb = commandBuffers[bufferIndex];
    cb.reset();
    cb.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    });
    return cb;
}

}
