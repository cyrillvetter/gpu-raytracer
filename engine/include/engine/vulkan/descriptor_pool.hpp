#pragma once

#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <cstddef>
#include <vector>

namespace engine::vulkan {

struct DescriptorPoolInfo final {
    vk::DescriptorPoolCreateFlags flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    uint32_t maxSets = 1;
};

class DescriptorPool final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(DescriptorPool)
    DescriptorPool() = delete;
    DescriptorPool(std::nullptr_t) : pool(nullptr) {}

    [[nodiscard]] const vk::raii::DescriptorPool& get() const noexcept { return pool; }

friend class DescriptorPoolBuilder;
private:
    vk::raii::DescriptorPool pool;

    explicit DescriptorPool(
        const Device& device,
        const std::vector<vk::DescriptorPoolSize>& poolSizes,
        const DescriptorPoolInfo& info
    );
};

class DescriptorPoolBuilder final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(DescriptorPoolBuilder)
    DescriptorPoolBuilder() = default;

    DescriptorPoolBuilder& add(vk::DescriptorType type, uint32_t count) noexcept;
    DescriptorPool build(const Device& device, const DescriptorPoolInfo& info);

private:
    std::vector<vk::DescriptorPoolSize> poolSizes;
};

}
