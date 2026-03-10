#pragma once

#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <cstddef>

namespace engine::vulkan {

struct CommandBuffersInfo final {
    uint32_t count = 1;
};

class CommandBuffers final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(CommandBuffers)
    CommandBuffers() = delete;
    CommandBuffers(std::nullptr_t) {}

    explicit CommandBuffers(const Device& device, const CommandPool& commandPool, const CommandBuffersInfo& info);

    const vk::raii::CommandBuffer& beginRecording(size_t bufferIndex) const;

    [[nodiscard]] const vk::raii::CommandBuffer& at(size_t bufferIndex) const { return commandBuffers[bufferIndex]; }
    [[nodiscard]] uint32_t getCount() const noexcept { return count; }

private:
    uint32_t count;
    std::vector<vk::raii::CommandBuffer> commandBuffers;
};

}
