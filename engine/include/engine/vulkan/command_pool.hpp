#pragma once

#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace engine::vulkan {

struct CommandPoolInfo final {
    uint32_t queueFamilyIndex = ~0;
    bool shortLived = false;
};

class CommandPool final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(CommandPool)
    CommandPool() = delete;

    explicit CommandPool(const Device& device, const CommandPoolInfo& info);

    [[nodiscard]] const vk::raii::CommandPool& get() const noexcept { return commandPool; }

private:
    vk::raii::CommandPool commandPool;
};

}
