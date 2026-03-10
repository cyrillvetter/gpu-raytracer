#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <stdexcept>

namespace engine::vulkan {

PushConstantSet::PushConstantSet(std::vector<vk::PushConstantRange> ranges)
    : ranges(std::move(ranges)) {}

PushConstantSetBuilder& PushConstantSetBuilder::add(size_t size, vk::ShaderStageFlags stageFlags) {
    bool overlaps = std::ranges::any_of(ranges, [stageFlags](const auto& range) {
        return (stageFlags & range.stageFlags) != vk::ShaderStageFlags{};
    });

    if (overlaps) {
        throw std::runtime_error("A shader stage only supports one push constant.");
    }

    uint32_t u32Size = static_cast<uint32_t>(size);
    ranges.emplace_back(vk::PushConstantRange{
        .stageFlags = stageFlags,
        .offset = offset,
        .size = u32Size
    });

    offset = nextOffset;
    nextOffset += u32Size;
    return *this;
}

PushConstantSet PushConstantSetBuilder::build() const noexcept {
    return PushConstantSet(std::move(ranges));
}

}
