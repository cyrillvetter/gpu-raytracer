#include <engine/engine.hpp>
#include <engine/vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <optional>

namespace {

uint32_t alignedSize(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

}

namespace engine::vulkan {

ShaderBindingTables::ShaderBindingTables(
    const Device& device,
    const Pipeline& pipeline,
    const ShaderBindingTablesInfo& info
) {
    auto rtProperties = pipeline.getProperties();

    // Calculate shader binding table size:
    handleSize = rtProperties.shaderGroupHandleSize;
    handleSizeAligned = alignedSize(handleSize, rtProperties.shaderGroupHandleAlignment);
    const uint32_t groupCount = pipeline.getGroupCount();
    const uint32_t sbtSize = handleSize * handleSizeAligned;

    // Shader group handles:
    handleStorage = pipeline.get().getRayTracingShaderGroupHandlesKHR<uint8_t>(0, groupCount, sbtSize);

    // Create shader binding tables.
    createTable(device, raygenTable, 1);
    createTable(device, hitTable, info.hitCount);
    createTable(device, missTable, info.missCount);
    createTable(device, callableTable, info.callableCount);
}

void ShaderBindingTables::stageTables(const vk::raii::CommandBuffer& commandBuffer) {
    stageTable(raygenTable.table, commandBuffer);
    stageTable(hitTable.table, commandBuffer);
    stageTable(missTable.table, commandBuffer);
    stageTable(callableTable.table, commandBuffer);
}

void ShaderBindingTables::unstageTables() {
    unstageTable(raygenTable.table);
    unstageTable(hitTable.table);
    unstageTable(missTable.table);
    unstageTable(callableTable.table);
}

void ShaderBindingTables::traceRays(const vk::raii::CommandBuffer& commandBuffer, uint32_t width, uint32_t height) const {
    commandBuffer.traceRaysKHR(raygenTable.region, missTable.region, hitTable.region, callableTable.region, width, height, 1);
}

void ShaderBindingTables::createTable(
    const Device& device,
    SBT& sbt,
    uint32_t count
) {
    if (count == 0) {
        return;
    }

    // Create sbt buffer.
    sbt.table = StagedBuffer(device, StagedBufferInfo{
        .stride = handleSize,
        .count = count,
        .usageFlags = vk::BufferUsageFlagBits::eShaderBindingTableKHR
    }, handleStorage.data() + handleSizeAligned * currentStorageOffset);

    // Specify sbt region.
    sbt.region.setSize(count * handleSizeAligned);
    sbt.region.setStride(handleSizeAligned);
    sbt.region.setDeviceAddress(sbt.table->getAddress());

    currentStorageOffset += count;
}

void ShaderBindingTables::stageTable(std::optional<StagedBuffer>& table, const vk::raii::CommandBuffer& commandBuffer) {
    if (table.has_value()) {
        table->stage(commandBuffer);
    }
}

void ShaderBindingTables::unstageTable(std::optional<StagedBuffer>& table) {
    if (table.has_value()) {
        table->unstage();
    }
}

}
