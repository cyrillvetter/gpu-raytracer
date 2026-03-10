#pragma once

#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <vector>
#include <optional>
#include <cstddef>

namespace engine::vulkan {

struct ShaderBindingTablesInfo final {
    uint32_t hitCount = 0;
    uint32_t missCount = 0;
    uint32_t callableCount = 0;
};

class ShaderBindingTables final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(ShaderBindingTables)
    ShaderBindingTables() = delete;
    ShaderBindingTables(std::nullptr_t) {}

    explicit ShaderBindingTables(
        const Device& device,
        const Pipeline& pipeline,
        const ShaderBindingTablesInfo& info
    );

    void traceRays(const vk::raii::CommandBuffer& commandBuffer, uint32_t width, uint32_t height) const;

    void stageTables(const vk::raii::CommandBuffer& commandBuffer);
    void unstageTables();

private:
    struct SBT final {
        std::optional<StagedBuffer> table = std::nullopt;
        vk::StridedDeviceAddressRegionKHR region{};
    };

    uint32_t currentStorageOffset = 0;

    std::vector<uint8_t> handleStorage;
    uint32_t handleSize;
    uint32_t handleSizeAligned;

    SBT raygenTable;
    SBT missTable;
    SBT hitTable;
    SBT callableTable;

    void createTable(const Device& device, SBT& sbt, uint32_t count);

    static void stageTable(std::optional<StagedBuffer>& table, const vk::raii::CommandBuffer& commandBuffer);
    static void unstageTable(std::optional<StagedBuffer>& table);
};

}
