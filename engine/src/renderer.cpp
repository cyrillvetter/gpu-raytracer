#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <stdexcept>
#include <unordered_map>
#include <algorithm>

namespace engine {

Renderer::Renderer(
    Device& device,
    const ShaderResources& shaderResources,
    const ShaderStages& shaderStages,
    const RenderInfo& info
)
    : commandPool(device.getDevice(), vulkan::CommandPoolInfo{
          .queueFamilyIndex = device.getDevice().getQueueFamilyIndex(),
          .shortLived = false
      }),
      commandBuffers(device.getDevice(), commandPool, vulkan::CommandBuffersInfo{
          .count = device.getSwapchain().getImageCount()
      }),
      pushConstantSet(shaderResources.pushConstantSetBuilder.build()),
      synchronizationFence(device.getDevice()) {
    setupDescriptors(device, shaderResources);
    setupPipeline(device, shaderStages, info);
    setupSemaphores(device);
    beginRecording(device);
}

void Renderer::render(Device& device, const Texture& texture) {
    const auto& logicalDevice = device.getDevice();
    synchronizationFence.reset(logicalDevice);

    const auto& computeCommandBuffer = commandBuffers.at(activeComputeBufferIndex);

    if (!initialized) {
        initialize(device, computeCommandBuffer);
    } else {
        device.restage(computeCommandBuffer);
    }

    // Record commands for rendering a single frame.
    pipeline.bind(computeCommandBuffer);
    descriptorSet.bind(computeCommandBuffer, pipeline);

    auto& outputTexture = device.get(texture);
    device.shaderBindingTables.traceRays(computeCommandBuffer, outputTexture.getWidth(), outputTexture.getHeight());

    if (device.renderTarget.srgb) {
        outputTexture.blitToSwapchain(device.getSwapchain(), computeCommandBuffer, swapchainAcquisition);
    } else {
        outputTexture.copyToSwapchain(device.getSwapchain(), computeCommandBuffer, swapchainAcquisition);
    }

    computeCommandBuffer.end();

    logicalDevice
        .getQueue()
        .submit(vk::SubmitInfo{
            .commandBufferCount = 1,
            .pCommandBuffers = &*computeCommandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &*renderFinishedSemaphores[semaphoreIndex].get()
        }, synchronizationFence.get());

    std::array<vk::Semaphore, 2> presentWaitSemaphores = {
        presentCompleteSemaphores[semaphoreIndex].get(),
        renderFinishedSemaphores[semaphoreIndex].get()
    };

    device.getSwapchain().present(logicalDevice, swapchainAcquisition, presentWaitSemaphores);

    // Wait for frame render completion.
    synchronizationFence.waitIndefinitely(logicalDevice);

    if (!cleanedUp) {
        device.unstage();
        cleanedUp = true;
    }

    semaphoreIndex = (semaphoreIndex + 1) % presentCompleteSemaphores.size();
    beginRecording(device);
}

void Renderer::initialize(Device& device, const vk::raii::CommandBuffer& commandBuffer) {
    if (initialized) return;

    device.initialize(commandBuffer);
    initialized = true;
}

void Renderer::beginRecording(const Device& device) {
    swapchainAcquisition = device.getSwapchain().acquireNextImage(presentCompleteSemaphores[semaphoreIndex]);
    activeComputeBufferIndex = swapchainAcquisition.imageIndex;
    commandBuffers.beginRecording(activeComputeBufferIndex);
}

void Renderer::setupDescriptors(const Device& device, const ShaderResources& shaderResources) {
    std::unordered_map<vk::DescriptorType, size_t> descriptorCounts;
    vulkan::DescriptorSetBuilder descriptorSetBuilder;

    for (const auto& resources : shaderResources.descriptors) {
        descriptorCounts[resources.type] += resources.resources.size();

        descriptorSetBuilder.beginBinding(resources.stageFlags, resources.type);
        for (const auto& resource : resources.resources) {
            descriptorSetBuilder.add(device.get(resource));
        }
        descriptorSetBuilder.endBinding();
    }

    vulkan::DescriptorPoolBuilder descriptorPoolBuilder;
    for (auto [type, count] : descriptorCounts) {
        descriptorPoolBuilder.add(type, static_cast<uint32_t>(count));
    }

    descriptorPool = descriptorPoolBuilder.build(device.getDevice(), vulkan::DescriptorPoolInfo{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 1
    });
    descriptorSet = descriptorSetBuilder.build(device.getDevice(), descriptorPool);
}

void Renderer::setupPipeline(Device& device, const ShaderStages& shaderStages, const RenderInfo& info) {
    if (!shaderStages.hasRaygen) {
        throw std::runtime_error("Exactly one raygen shader stage is expected.");
    }

    for (std::string filename : shaderStages.shaders) {
        shaderModules.insert({
            filename,
            vulkan::ShaderModule(device.getDevice(), filename)
        });
    }

    auto shaderGroups = std::move(shaderStages.groups);
    std::stable_sort(shaderGroups.begin(), shaderGroups.end(), [](const auto& group1, const auto& group2) {
        return group1.groupType < group2.groupType;
    });

    uint32_t hitGroupCount = 0;
    uint32_t missCount = 0;
    uint32_t callableCount = 0;

    std::vector<vk::PipelineShaderStageCreateInfo> stageInfos;
    stageInfos.reserve(shaderGroups.size() * 2); // NOTE: On average, each shader group contains two stages.
    uint32_t stageIndex = 0;

    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> groupInfos;
    groupInfos.reserve(shaderGroups.size());

    for (const auto& group : shaderGroups) {
        vk::RayTracingShaderGroupCreateInfoKHR groupInfo{};

        switch (group.groupType) {
            case ShaderStages::GroupType::Triangle:
                groupInfo.setType(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup);
                ++hitGroupCount;
                break;
            case ShaderStages::GroupType::Procedural:
                groupInfo.setType(vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup);
                ++hitGroupCount;
                break;
            case ShaderStages::GroupType::Raygen:
                groupInfo.setType(vk::RayTracingShaderGroupTypeKHR::eGeneral);
                break;
            case ShaderStages::GroupType::Miss:
                groupInfo.setType(vk::RayTracingShaderGroupTypeKHR::eGeneral);
                ++missCount;
                break;
            case ShaderStages::GroupType::Callable:
                groupInfo.setType(vk::RayTracingShaderGroupTypeKHR::eGeneral);
                ++callableCount;
                break;
            default: break;
        }

        for (size_t i = 0; i < group.count; ++i) {
            const auto& stage = group.stages[i];

            stageInfos.push_back(vk::PipelineShaderStageCreateInfo{
                .stage = stage.stageFlag,
                .module = shaderModules.at(stage.filename).get(),
                .pName = stage.entryPoint.c_str()
            });

            switch (stage.stageFlag) {
                case vk::ShaderStageFlagBits::eClosestHitKHR:
                    groupInfo.setClosestHitShader(stageIndex);
                    break;
                case vk::ShaderStageFlagBits::eAnyHitKHR:
                    groupInfo.setAnyHitShader(stageIndex);
                    break;
                case vk::ShaderStageFlagBits::eIntersectionKHR:
                    groupInfo.setIntersectionShader(stageIndex);
                    break;
                default:
                    groupInfo.setGeneralShader(stageIndex);
                    break;
            }

            ++stageIndex;
        }

        groupInfos.push_back(groupInfo);
    }

    stageInfos.shrink_to_fit();
    pipeline = vulkan::Pipeline(
        device.getDevice(),
        descriptorSet,
        &pushConstantSet,
        stageInfos,
        groupInfos,
        vulkan::PipelineBuildInfo{
            .maxRecursionDepth = static_cast<uint32_t>(info.maxRecursionDepth)
        }
    );
    device.createShaderBindingTables(pipeline, vulkan::ShaderBindingTablesInfo{
        .hitCount = hitGroupCount,
        .missCount = missCount,
        .callableCount = callableCount
    });
}

void Renderer::setupSemaphores(const Device& device) {
    uint32_t imageCount = device.getSwapchain().getImageCount();
    presentCompleteSemaphores.reserve(imageCount);
    renderFinishedSemaphores.reserve(imageCount);

    for (size_t i = 0; i < device.getSwapchain().getImageCount(); ++i) {
        presentCompleteSemaphores.emplace_back(device.getDevice());
        renderFinishedSemaphores.emplace_back(device.getDevice());
    }
}

}
