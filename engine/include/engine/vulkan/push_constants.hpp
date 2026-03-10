#pragma once

#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vector>

namespace engine::vulkan {

class PushConstantSet final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(PushConstantSet)

    [[nodiscard]] const std::vector<vk::PushConstantRange>& get() const noexcept { return ranges; }
    [[nodiscard]] uint32_t getCount() const noexcept { return ranges.size(); }

private:
    friend class PushConstantSetBuilder;

    std::vector<vk::PushConstantRange> ranges;
    explicit PushConstantSet(std::vector<vk::PushConstantRange> ranges);
};

class PushConstantSetBuilder final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(PushConstantSetBuilder)
    PushConstantSetBuilder() = default;

    PushConstantSetBuilder& add(size_t size, vk::ShaderStageFlags stageFlags);
    PushConstantSet build() const noexcept;

    [[nodiscard]] uint32_t getCurrentOffset() const noexcept { return offset; }

private:
    std::vector<vk::PushConstantRange> ranges;
    uint32_t offset = 0;
    uint32_t nextOffset = 0;
};

}
