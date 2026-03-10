#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <vector>

namespace engine::vulkan {

Descriptor::~Descriptor() {}

DescriptorSet::DescriptorSet(
    const Device& device,
    const DescriptorPool& pool,
    std::vector<vk::DescriptorSetLayoutBinding> layoutBindings,
    std::vector<vk::WriteDescriptorSet> descriptorWrites
) : bindings(std::move(layoutBindings)), writes(std::move(descriptorWrites)) {
    // Create descriptor set layout:
    uint32_t bindingCount = static_cast<uint32_t>(bindings.size());

    vk::DescriptorSetLayoutBindingFlagsCreateInfoEXT setLayoutBindingFlags{
        .bindingCount = bindingCount,
    };

    std::vector<vk::DescriptorBindingFlags> descriptorBindingFlags = {
        static_cast<vk::DescriptorBindingFlags>(0),
        vk::DescriptorBindingFlagBits::eVariableDescriptorCount
    };

    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{
        .bindingCount = bindingCount,
        .pBindings = bindings.data()
    };
    layout = vk::raii::DescriptorSetLayout(device.get(), descriptorSetLayoutInfo);

    // Create descriptor set:
    vk::DescriptorSetAllocateInfo descriptorSetAllocInfo{
        .descriptorPool = pool.get(),
        .descriptorSetCount = 1,
        .pSetLayouts = &*layout
    };
    set = std::move(vk::raii::DescriptorSets(device.get(), descriptorSetAllocInfo).front());

    for (auto& write : writes) {
        write.setDstSet(set);
    }

    // Update descriptor sets:
    device.get().updateDescriptorSets(writes, nullptr);
}

void DescriptorSet::bind(const vk::raii::CommandBuffer& commandBuffer, const Pipeline& pipeline) const {
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, *pipeline.getLayout(), 0, *set, nullptr);
}

void DescriptorSetBuilder::beginBinding(vk::ShaderStageFlags stageFlags, vk::DescriptorType type) noexcept {
    currentType = type;
    bindings.emplace_back(vk::DescriptorSetLayoutBinding{
        .binding = bindingIndex,
        .descriptorType = type,
        .descriptorCount = 0,
        .stageFlags = stageFlags
    });
}

void DescriptorSetBuilder::add(const Descriptor& descriptor) noexcept {
    vk::WriteDescriptorSet writer{
        .dstBinding = bindingIndex,
        .dstArrayElement = arrayIndex,
        .descriptorCount = 1,
        .descriptorType = currentType
    };
    descriptor.writeSet(writer);

    bindings.back().descriptorCount++;
    arrayIndex++;
    descriptorWrites.push_back(writer);
}

void DescriptorSetBuilder::endBinding() noexcept {
    bindingIndex++;
    arrayIndex = 0;
}

DescriptorSet DescriptorSetBuilder::build(const Device& device, const DescriptorPool& pool) {
    return DescriptorSet(device, pool, std::move(bindings), std::move(descriptorWrites));
}

}
