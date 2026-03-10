#pragma once

#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <cstddef>

namespace engine::vulkan {

struct PipelineBuildInfo final {
    uint32_t maxRecursionDepth = 1;
};

class Pipeline final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(Pipeline)
    Pipeline() = delete;
    Pipeline(std::nullptr_t) {}

    explicit Pipeline(
        const Device& device,
        const DescriptorSet& descriptorSet,
        const PushConstantSet* pushConstants,
        const std::vector<vk::PipelineShaderStageCreateInfo>& shaderStages,
        const std::vector<vk::RayTracingShaderGroupCreateInfoKHR>& shaderGroups,
        const PipelineBuildInfo& info
    );

    void bind(const vk::raii::CommandBuffer& commandBuffer) const;

    [[nodiscard]] const vk::raii::Pipeline& get() const noexcept { return pipeline; }
    [[nodiscard]] const vk::raii::PipelineLayout& getLayout() const noexcept { return pipelineLayout; }
    [[nodiscard]] uint32_t getGroupCount() const noexcept { return groupCount; }
    [[nodiscard]] const vk::PhysicalDeviceRayTracingPipelinePropertiesKHR& getProperties() const noexcept { return properties; }

private:
    uint32_t groupCount = 0;

    vk::raii::PipelineLayout pipelineLayout = nullptr;
    vk::raii::Pipeline pipeline = nullptr;

    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR properties;
};

}
