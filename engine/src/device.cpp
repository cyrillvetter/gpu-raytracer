#include <engine/engine.hpp>
#include <engine/vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <variant>
#include <iostream>

namespace engine {

Device::Device(const Context& context, const RenderTarget& renderTarget)
    : surface(context.getInstance(), renderTarget.surface),
      device(context.getInstance(), surface),
      swapchain(device, surface, vulkan::SwapchainInfo{
          .width = static_cast<uint32_t>(renderTarget.dimensions.width),
          .height = static_cast<uint32_t>(renderTarget.dimensions.height),
          .vsync = renderTarget.vsync,
          .srgb = renderTarget.srgb
      }),
      renderTarget(renderTarget) {
    std::cout << "Using physical device: " << device.getProperties().deviceName
        << ", device type: " << vk::to_string(device.getProperties().deviceType)
        << std::endl;
}

Texture Device::texture(const TextureInfo& info) {
    images.emplace_back(device, vulkan::ImageInfo{
        .dimensions = info.dimensions,
        .format = info.format,
        .usageFlags = info.usageFlags
    });
    return Texture(imageIndex++);
}

AccelerationStructure Device::accelerationStructure(const AccelerationStructureBuilder& builder) {
    blas.reserve(builder.aabbStructureCount + builder.triangleStructureCount);
    AABB* mapped = nullptr;

    if (builder.aabbStructureCount > 0) {
        hasProcedural = true;
        aabbBuffer = vulkan::StagedBuffer(device, vulkan::StagedBufferInfo{
            .stride = sizeof(AABB),
            .count = static_cast<BufferSize>(builder.aabbStructureCount),
            .usageFlags =
                vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
                vk::BufferUsageFlagBits::eTransferDst,
            .updatable = false,
        });
        mapped = reinterpret_cast<AABB*>(aabbBuffer.getMapped());
    }

    size_t aabbIndex = 0;
    for (const auto& var : builder.structures) {
        if (std::holds_alternative<AABB>(var)) {
            mapped[aabbIndex] = std::get<AABB>(var);
            blas.emplace_back(device, aabbBuffer, static_cast<uint32_t>(aabbIndex));
            aabbIndex++;
        } else {
            const auto& structure = std::get<TriangleAccelerationStructure>(var);
            blas.emplace_back(device, storageBuffers[structure.vertexBuffer.index], storageBuffers[structure.indexBuffer.index]);
        }
    }

    vulkan::InstancesBufferBuilder instancesBuilder;
    for (const auto& info : builder.instanceInfos) {
        instancesBuilder.add(blas[info.blasIndex], info.structureId, info.hitShaderGroupIndex, info.transform);
    }
    instancesBuffer = instancesBuilder.build(device);

    tlas = vulkan::TopLevelAccelerationStructure(device, instancesBuffer);
    return AccelerationStructure();
}

void Device::wait() const {
    device.waitForDevice();
}

vulkan::Image& Device::get(const Texture& resource) {
    return images[resource.index];
}

vulkan::TopLevelAccelerationStructure& Device::get(const AccelerationStructure&) {
    return tlas;
}

const vulkan::Descriptor& Device::get(const Resource& resource) const {
    switch (resource.type) {
        case vk::DescriptorType::eStorageBuffer: return storageBuffers[resource.index];
        case vk::DescriptorType::eUniformBuffer: return uniformBuffers[resource.index];
        case vk::DescriptorType::eStorageImage: return images[resource.index];
        case vk::DescriptorType::eCombinedImageSampler: return samplers[resource.index];
        case vk::DescriptorType::eAccelerationStructureKHR: return tlas;
        default: throw std::runtime_error("Unknown resource type.");
    }
}

void Device::createShaderBindingTables(const vulkan::Pipeline& pipeline, const vulkan::ShaderBindingTablesInfo& info) {
    shaderBindingTables = vulkan::ShaderBindingTables(device, pipeline, info);
}

void Device::initialize(const vk::raii::CommandBuffer& commandBuffer) {
    // Image transitions.
    std::vector<vk::ImageMemoryBarrier2> initialImageTransitions;
    initialImageTransitions.reserve(images.size() + samplers.size());

    for (const auto& image : images) {
        initialImageTransitions.push_back(image.transitionImageLayout(vulkan::ImageTransitionInfo{
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eGeneral,
            .srcStageMask = vk::PipelineStageFlagBits2::eNone,
            .srcAccessMask = vk::AccessFlagBits2::eNone,
            .dstStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
            .dstAccessMask = vk::AccessFlagBits2::eShaderStorageWrite
        }));
    }
    for (const auto& sampler : samplers) {
        initialImageTransitions.push_back(sampler.transitionImageLayout(vulkan::ImageTransitionInfo{
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eTransferDstOptimal,
            .srcStageMask = vk::PipelineStageFlagBits2::eNone,
            .srcAccessMask = vk::AccessFlagBits2::eNone,
            .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
            .dstAccessMask = vk::AccessFlagBits2::eTransferWrite
        }));
    }
    commandBuffer.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = static_cast<uint32_t>(initialImageTransitions.size()),
        .pImageMemoryBarriers = initialImageTransitions.data()
    });

    // Transfer all buffers to device.
    for (const auto& storageBuffer : storageBuffers) {
        storageBuffer.stage(commandBuffer);
    }
    for (const auto& uniformBuffer : uniformBuffers) {
        uniformBuffer.stage(commandBuffer);
    }
    for (const auto& sampler : samplers) {
        sampler.stage(commandBuffer);
    }
    if (hasProcedural) {
        aabbBuffer.stage(commandBuffer);
    }
    instancesBuffer.stage(commandBuffer);
    shaderBindingTables.stageTables(commandBuffer);

    // Transition sampled textures.
    if (!samplers.empty()) {
        std::vector<vk::ImageMemoryBarrier2> samplerReadBarriers;
        samplerReadBarriers.reserve(samplers.size());

        for (const auto& sampler : samplers) {
            samplerReadBarriers.push_back(sampler.transitionImageLayout(vulkan::ImageTransitionInfo{
                .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
                .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead
            }));
        }

        commandBuffer.pipelineBarrier2(vk::DependencyInfo{
            .imageMemoryBarrierCount = static_cast<uint32_t>(samplers.size()),
            .pImageMemoryBarriers = samplerReadBarriers.data()
        });
    }

    // Wait for all transfers to complete.
    const vk::MemoryBarrier2 transferBarrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
        .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
        .dstAccessMask = vk::AccessFlagBits2::eTransferRead
    };
    commandBuffer.pipelineBarrier2(vk::DependencyInfo{
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &transferBarrier
    });

    // Build all bottom level acceleration structures.
    for (auto& blas : blas) {
        blas.build(commandBuffer);
    }

    // Wait until all bottom level acceleration structures are built.
    const vk::MemoryBarrier2 buildBarrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
        .srcAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR,
        .dstStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
        .dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR
    };
    commandBuffer.pipelineBarrier2(vk::DependencyInfo{
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &buildBarrier
    });

    // Build single top level acceleration structure.
    tlas.build(commandBuffer);

    // Wait until the top level acceleration structure is built.
    commandBuffer.pipelineBarrier2(vk::DependencyInfo{
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &buildBarrier
    });
}

void Device::unstage() {
    for (auto& storageBuffer : storageBuffers)
        storageBuffer.unstage();
    for (auto& uniformBuffer : uniformBuffers)
        uniformBuffer.unstage();
    for (auto& sampler : samplers)
        sampler.unstage();
    for (auto& blas : blas)
        blas.clean();
    tlas.clean();
    if (hasProcedural) {
        aabbBuffer.unstage();
    }
    instancesBuffer.unstage();
    shaderBindingTables.unstageTables();
}

void Device::restage(const vk::raii::CommandBuffer& commandBuffer) {
    if (constantBufferUpdates.empty()) {
        return;
    }

    // Transfer all updates uniform buffers to device local memory.
    for (size_t i : constantBufferUpdates) {
        uniformBuffers[i].stage(commandBuffer);
    }
    constantBufferUpdates.clear();

    // Wait for all transfers to be completed.
    const vk::MemoryBarrier2 transferBarrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
        .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
        .dstAccessMask = vk::AccessFlagBits2::eTransferRead
    };
    commandBuffer.pipelineBarrier2(vk::DependencyInfo{
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &transferBarrier
    });
}

}
