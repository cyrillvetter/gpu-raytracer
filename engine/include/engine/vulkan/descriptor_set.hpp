#pragma once

#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <vector>
#include <cstddef>

namespace engine::vulkan {

class Descriptor {
public:
    virtual ~Descriptor() = 0;
    virtual void writeSet(vk::WriteDescriptorSet& writer) const = 0;

protected:
    Descriptor() = default;
};

class Pipeline;

class DescriptorSet final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(DescriptorSet)

    DescriptorSet() = delete;
    DescriptorSet(std::nullptr_t) {}

    void bind(const vk::raii::CommandBuffer& commandBuffer, const Pipeline& pipeline) const;

    [[nodiscard]] const vk::raii::DescriptorSetLayout& getLayout() const noexcept { return layout; }
    [[nodiscard]] const vk::raii::DescriptorSet& get() const noexcept { return set; }

private:
    friend class DescriptorSetBuilder;

    vk::raii::DescriptorSetLayout layout = nullptr;
    vk::raii::DescriptorSet set = nullptr;

    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    std::vector<vk::WriteDescriptorSet> writes;

    explicit DescriptorSet(
        const Device& device,
        const DescriptorPool& pool,
        std::vector<vk::DescriptorSetLayoutBinding> layoutBindings,
        std::vector<vk::WriteDescriptorSet> descriptorWrites
    );
};

class DescriptorSetBuilder final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(DescriptorSetBuilder)
    DescriptorSetBuilder() = default;

    void beginBinding(vk::ShaderStageFlags stageFlags, vk::DescriptorType type) noexcept;
    void add(const Descriptor& descriptor) noexcept;
    void endBinding() noexcept;

    DescriptorSet build(const Device& device, const DescriptorPool& pool);

private:
    uint32_t bindingIndex = 0;
    uint32_t arrayIndex = 0;
    vk::DescriptorType currentType{};

    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    std::vector<vk::WriteDescriptorSet> descriptorWrites;
};

}
