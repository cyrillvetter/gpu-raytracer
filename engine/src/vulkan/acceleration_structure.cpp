#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace {

vk::TransformMatrixKHR toVulkanTransformMatrix(const glm::mat4x3& mat) {
    vk::TransformMatrixKHR transformMatrix{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 4; ++col) {
            transformMatrix.matrix[row][col] = mat[col][row];
        }
    }

    return transformMatrix;
}

}

namespace engine::vulkan {

AccelerationStructureBase::AccelerationStructureBase(
    const Device& device,
    uint32_t primitiveCount,
    uint32_t primitiveOffset,
    vk::AccelerationStructureGeometryKHR geometry,
    vk::AccelerationStructureTypeKHR type
) : primitiveCount(primitiveCount), primitiveOffset(primitiveOffset), type(type), accelerationStructureGeometry(geometry) {
    buildGeometryInfo.setType(type);
    buildGeometryInfo.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace); // TODO: Make fastBuild, fastTrace and low memory configurable.
    buildGeometryInfo.setGeometries(accelerationStructureGeometry);

    // Create required buffer sizes.
    vk::AccelerationStructureBuildSizesInfoKHR buildSizesInfo = device.get().getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice,
        buildGeometryInfo,
        primitiveCount
    );

    // Create storage buffer.
    vk::DeviceSize structureSize = buildSizesInfo.accelerationStructureSize;
    storageBuffer = UninitializedBuffer(device, UninitializedBufferInfo{
        .stride = structureSize,
        .count = 1,
        .usageFlags = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR,
        .memoryFlags = vk::MemoryPropertyFlagBits::eDeviceLocal
    });

    // Create acceleration structure.
    const vk::AccelerationStructureCreateInfoKHR accelerationStructureInfo{
        .buffer = storageBuffer.get(),
        .size = structureSize,
        .type = type
    };
    accelerationStructure = vk::raii::AccelerationStructureKHR(device.get(), accelerationStructureInfo);

    // Create scratch buffer (used during acceleration structure construction).
    vk::DeviceSize scratchSize = buildSizesInfo.buildScratchSize;
    scratchBuffer = UninitializedBuffer(device, UninitializedBufferInfo{
        .stride = scratchSize,
        .count = 1,
        .usageFlags = vk::BufferUsageFlagBits::eStorageBuffer,
        .memoryFlags = vk::MemoryPropertyFlagBits::eDeviceLocal
    });

    buildGeometryInfo.setScratchData(scratchBuffer.getAddress());
    buildGeometryInfo.setDstAccelerationStructure(*accelerationStructure);

    writeDescriptorAccelerationStructure.setAccelerationStructures(*accelerationStructure);
}

void AccelerationStructureBase::build(const vk::raii::CommandBuffer& commandBuffer) {
    buildGeometryInfo.setGeometries(accelerationStructureGeometry);
    buildGeometryInfo.setDstAccelerationStructure(accelerationStructure);

    // Build the acceleration structure.
    vk::AccelerationStructureBuildRangeInfoKHR buildRangeInfo{
        .primitiveCount = primitiveCount,
        .primitiveOffset = primitiveOffset,
        .firstVertex = 0,
        .transformOffset = 0
    };
    commandBuffer.buildAccelerationStructuresKHR(buildGeometryInfo, &buildRangeInfo);
}

void AccelerationStructureBase::clean() {
    scratchBuffer = nullptr;
}

AccelerationStructureBase::AccelerationStructureBase(AccelerationStructureBase&& other) noexcept
    : primitiveCount(std::exchange(other.primitiveCount, 0)),
      primitiveOffset(std::exchange(other.primitiveOffset, 0)),
      type(std::exchange(other.type, vk::AccelerationStructureTypeKHR{})),
      storageBuffer(std::move(other.storageBuffer)),
      scratchBuffer(std::move(other.scratchBuffer)),
      accelerationStructure(std::move(other.accelerationStructure)),
      accelerationStructureGeometry(std::move(other.accelerationStructureGeometry)),
      buildGeometryInfo(std::move(other.buildGeometryInfo)),
      writeDescriptorAccelerationStructure(std::move(other.writeDescriptorAccelerationStructure)) {
    buildGeometryInfo.setGeometries(accelerationStructureGeometry);
    buildGeometryInfo.setDstAccelerationStructure(*accelerationStructure);
    writeDescriptorAccelerationStructure.setAccelerationStructures(*accelerationStructure);
}

AccelerationStructureBase& AccelerationStructureBase::operator=(AccelerationStructureBase&& other) noexcept {
    if (this != &other) {
        primitiveCount = std::exchange(other.primitiveCount, 0);
        primitiveOffset = std::exchange(other.primitiveOffset, 0);
        type = std::exchange(other.type, vk::AccelerationStructureTypeKHR{});
        storageBuffer = std::move(other.storageBuffer);
        scratchBuffer = std::move(other.scratchBuffer);
        accelerationStructure = std::move(other.accelerationStructure);
        accelerationStructureGeometry = std::move(other.accelerationStructureGeometry);

        buildGeometryInfo = std::move(other.buildGeometryInfo);
        buildGeometryInfo.setGeometries(accelerationStructureGeometry);
        buildGeometryInfo.setDstAccelerationStructure(*accelerationStructure);

        writeDescriptorAccelerationStructure = std::move(other.writeDescriptorAccelerationStructure);
        writeDescriptorAccelerationStructure.setAccelerationStructures(*accelerationStructure);
    }
    return *this;
}

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(
    const Device& device,
    const Buffer& vertexBuffer,
    const Buffer& indexBuffer
) : AccelerationStructureBase(
        device,
        static_cast<uint32_t>(indexBuffer.getCount() / 3),
        0, // Primitive offset.
        createTriangleGeometry(vertexBuffer, indexBuffer),
        vk::AccelerationStructureTypeKHR::eBottomLevel
    ) {}

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(
    const Device& device,
    const Buffer& aabbBuffer,
    uint32_t primitiveOffset
) : AccelerationStructureBase(
        device,
        static_cast<uint32_t>(aabbBuffer.getCount()),
        static_cast<uint32_t>(aabbBuffer.getStride()) * primitiveOffset,
        createProceduralGeometry(aabbBuffer),
        vk::AccelerationStructureTypeKHR::eBottomLevel
    ) {}

vk::AccelerationStructureGeometryKHR BottomLevelAccelerationStructure::createTriangleGeometry(const Buffer& vertexBuffer, const Buffer& indexBuffer) {
    return vk::AccelerationStructureGeometryKHR{
        .geometryType = vk::GeometryTypeKHR::eTriangles,
        .geometry = vk::AccelerationStructureGeometryTrianglesDataKHR{
            .vertexFormat = vk::Format::eR32G32B32Sfloat, // TODO: Make the format configurable.
            .vertexData = vertexBuffer.getAddress(),
            .vertexStride = vertexBuffer.getStride(),
            .maxVertex = static_cast<uint32_t>(vertexBuffer.getCount()),
            .indexType = vk::IndexType::eUint32, // TODO: Make the index type configurable.
            .indexData = indexBuffer.getAddress()
        },
        .flags = vk::GeometryFlagBitsKHR::eOpaque // TODO: Make configurable.
    };
}

vk::AccelerationStructureGeometryKHR BottomLevelAccelerationStructure::createProceduralGeometry(const Buffer& aabbBuffer) {
    return vk::AccelerationStructureGeometryKHR{
        .geometryType = vk::GeometryTypeKHR::eAabbs,
        .geometry = vk::AccelerationStructureGeometryAabbsDataKHR{
            .data = aabbBuffer.getAddress(),
            .stride = aabbBuffer.getStride()
        },
        .flags = vk::GeometryFlagBitsKHR::eOpaque // TODO: Make configurable.
    };
}

InstancesBuffer InstancesBufferBuilder::build(const Device& device) {
    return InstancesBuffer(device, instances);
}

InstancesBufferBuilder& InstancesBufferBuilder::add(
    const BottomLevelAccelerationStructure& structure,
    uint32_t customId,
    uint32_t hitShaderGroupIndex,
    const glm::mat4x3& transform
) {
    vk::AccelerationStructureInstanceKHR instance{
        .transform = toVulkanTransformMatrix(transform),
        .instanceCustomIndex = customId,
        .mask = 0xFF, // TODO: Make configurable.
        .instanceShaderBindingTableRecordOffset = hitShaderGroupIndex,
        .accelerationStructureReference = structure.getAddress(),
    };
    instance.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable); // TODO: Make configurable.
    instances.push_back(instance);

    return *this;
}

InstancesBuffer::InstancesBuffer(const Device& device, const std::vector<vk::AccelerationStructureInstanceKHR>& instances) :
    StagedBuffer(device, StagedBufferInfo{
        .stride = static_cast<BufferSize>(sizeof(vk::AccelerationStructureInstanceKHR)),
        .count = static_cast<BufferSize>(instances.size()),
        .usageFlags = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
    }, instances.data()),
    primitiveCount(static_cast<uint32_t>(instances.size())) {}

vk::AccelerationStructureGeometryKHR TopLevelAccelerationStructure::createGeometry(const InstancesBuffer& instancesBuffer) {
    return vk::AccelerationStructureGeometryKHR{
        .geometryType = vk::GeometryTypeKHR::eInstances,
        .geometry = vk::AccelerationStructureGeometryInstancesDataKHR{
            .arrayOfPointers = false,
            .data = instancesBuffer.getAddress(),
        },
        .flags = vk::GeometryFlagBitsKHR::eOpaque
    };
}

TopLevelAccelerationStructure::TopLevelAccelerationStructure(
    const Device& device,
    const InstancesBuffer& instancesBuffer
) : AccelerationStructureBase(
        device,
        instancesBuffer.getCount(),
        0, // Primitive offset.
        createGeometry(instancesBuffer),
        vk::AccelerationStructureTypeKHR::eTopLevel
    ) {}

}
