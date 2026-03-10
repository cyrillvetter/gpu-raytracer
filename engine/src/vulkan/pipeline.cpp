#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <iostream>

namespace engine::vulkan {

Pipeline::Pipeline(
    const Device& device,
    const DescriptorSet& descriptorSet,
    const PushConstantSet* pushConstants,
    const std::vector<vk::PipelineShaderStageCreateInfo>& shaderStages,
    const std::vector<vk::RayTracingShaderGroupCreateInfoKHR>& shaderGroups,
    const PipelineBuildInfo& info
) : groupCount(static_cast<uint32_t>(shaderGroups.size())),
    properties(device
        .getPhysical()
        .getProperties2<vk::PhysicalDeviceProperties2KHR, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>()
        .get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>()) {
    if (info.maxRecursionDepth > properties.maxRayRecursionDepth) {
        std::cout << "WARNING: Requested maxRecursionDepth (" << info.maxRecursionDepth
            << ") exceeds device limits of (" << properties.maxRayRecursionDepth
            << "). Using the device limit." << std::endl;
    }

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &*descriptorSet.getLayout()
    };

    if (pushConstants) {
        pipelineLayoutInfo.setPushConstantRanges(pushConstants->get());
    }

    pipelineLayout = vk::raii::PipelineLayout(device.get(), pipelineLayoutInfo);

    vk::RayTracingPipelineCreateInfoKHR rtPipelineInfo{
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .groupCount = groupCount,
        .pGroups = shaderGroups.data(),
        .maxPipelineRayRecursionDepth = std::min(info.maxRecursionDepth, properties.maxRayRecursionDepth),
        .layout = *pipelineLayout
    };
    pipeline = device.get().createRayTracingPipelineKHR(nullptr, nullptr, rtPipelineInfo);
}

void Pipeline::bind(const vk::raii::CommandBuffer& commandBuffer) const {
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, *pipeline);
}

}
