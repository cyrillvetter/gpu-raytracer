#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vector>

namespace engine::vulkan {

DescriptorPool::DescriptorPool(
    const Device& device,
    const std::vector<vk::DescriptorPoolSize>& poolSizes,
    const DescriptorPoolInfo& info
) : pool(vk::raii::DescriptorPool(
        device.get(),
        vk::DescriptorPoolCreateInfo{
            .flags = info.flags,
            .maxSets = info.maxSets,
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data()
        })
    ) {}

DescriptorPoolBuilder& DescriptorPoolBuilder::add(vk::DescriptorType type, uint32_t count) noexcept {
    poolSizes.emplace_back(vk::DescriptorPoolSize{
        .type = type,
        .descriptorCount = count
    });

    return *this;
}

DescriptorPool DescriptorPoolBuilder::build(const Device& device, const DescriptorPoolInfo& info) {
    return DescriptorPool(device, poolSizes, info);
}

}
