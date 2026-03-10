#pragma once

#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>

#include <cstddef>
#include <vector>

namespace engine::vulkan {

class AccelerationStructureBase {
public:
    NON_COPYABLE(AccelerationStructureBase)
    AccelerationStructureBase(AccelerationStructureBase&& other) noexcept;
    AccelerationStructureBase& operator=(AccelerationStructureBase&& other) noexcept;

    AccelerationStructureBase() = delete;
    AccelerationStructureBase(std::nullptr_t) {}

    [[nodiscard]] BufferAddress getAddress() const noexcept { return storageBuffer.getAddress(); }

    void build(const vk::raii::CommandBuffer& commandBuffer);
    void clean();

protected:
    uint32_t primitiveCount;
    uint32_t primitiveOffset;
    vk::AccelerationStructureTypeKHR type;

    UninitializedBuffer storageBuffer = nullptr;
    UninitializedBuffer scratchBuffer = nullptr;

    vk::raii::AccelerationStructureKHR accelerationStructure = nullptr;

    vk::AccelerationStructureGeometryKHR accelerationStructureGeometry;
    vk::AccelerationStructureBuildGeometryInfoKHR buildGeometryInfo;
    vk::WriteDescriptorSetAccelerationStructureKHR writeDescriptorAccelerationStructure;

    explicit AccelerationStructureBase(
        const Device& device,
        uint32_t primitiveCount,
        uint32_t primitiveOffset,
        vk::AccelerationStructureGeometryKHR geometry,
        vk::AccelerationStructureTypeKHR type
    );
};

class BottomLevelAccelerationStructure final : public AccelerationStructureBase {
public:
    BottomLevelAccelerationStructure() = delete;
    explicit BottomLevelAccelerationStructure(const Device& device, const Buffer& vertexBuffer, const Buffer& indexBuffer);
    explicit BottomLevelAccelerationStructure(const Device& device, const Buffer& aabbBuffer, uint32_t primitiveOffset);

private:
    static vk::AccelerationStructureGeometryKHR createTriangleGeometry(const Buffer& vertexBuffer, const Buffer& indexBuffer);
    static vk::AccelerationStructureGeometryKHR createProceduralGeometry(const Buffer& aabbBuffer);
};

class InstancesBuffer final : public StagedBuffer {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(InstancesBuffer)
    InstancesBuffer() = delete;
    InstancesBuffer(std::nullptr_t) : StagedBuffer(nullptr) {}

    [[nodiscard]] uint32_t getCount() const noexcept { return primitiveCount; }

private:
    friend class InstancesBufferBuilder;
    uint32_t primitiveCount;

    explicit InstancesBuffer(const Device& device, const std::vector<vk::AccelerationStructureInstanceKHR>& instances);
};

class InstancesBufferBuilder final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(InstancesBufferBuilder)
    InstancesBufferBuilder() = default;

    InstancesBufferBuilder& add(
        const BottomLevelAccelerationStructure& structure,
        uint32_t customId,
        uint32_t hitShaderGroupIndex,
        const glm::mat4x3& transform = glm::mat4x3(1.0f)
    );
    InstancesBuffer build(const Device& device);

private:
    std::vector<vk::AccelerationStructureInstanceKHR> instances;
};

class TopLevelAccelerationStructure final : public AccelerationStructureBase, public Descriptor {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(TopLevelAccelerationStructure)
    TopLevelAccelerationStructure() = delete;
    TopLevelAccelerationStructure(std::nullptr_t) : AccelerationStructureBase(nullptr) {}

    void writeSet(vk::WriteDescriptorSet& writer) const override {
        writer.setPNext(&writeDescriptorAccelerationStructure);
    }

    explicit TopLevelAccelerationStructure(
        const Device& device,
        const InstancesBuffer& instancesBuffer
    );

private:
    static vk::AccelerationStructureGeometryKHR createGeometry(const InstancesBuffer& instancesBuffer);
};

}
