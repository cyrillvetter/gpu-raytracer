#pragma once

#include <engine/engine.hpp>
#include <engine/vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <stdexcept>
#include <unordered_set>

namespace engine {

// TODO: Add a second render target that renders to an image.
struct RenderTarget final {
    VkSurfaceKHR surface;
    Dimensions dimensions;
    bool vsync = true;
    bool srgb = true;
};

class Device final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(Device)
    Device() = delete;

    explicit Device(const Context& context, const RenderTarget& renderTarget);

    template <typename T>
    StructuredBuffer<T> structuredBuffer(size_t count, const StructuredBufferInfo& info = {}) {
        storageBuffers.emplace_back(device, vulkan::StagedBufferInfo{
            .stride = static_cast<BufferSize>(sizeof(T)),
            .count = static_cast<BufferSize>(count),
            .usageFlags =
                vk::BufferUsageFlagBits::eStorageBuffer |
                (info.accelerationStructureInput ? vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR : vk::BufferUsageFlags{})
        });
        return StructuredBuffer<T>(
            storageBufferIndex++,
            reinterpret_cast<T*>(storageBuffers.back().getMapped()),
            storageBuffers.back().getSize()
        );
    }

    template <typename T>
    ConstantBuffer<T> constantBuffer(const ConstantBufferInfo& info = {}) {
        uniformBuffers.emplace_back(device, vulkan::StagedBufferInfo{
            .stride = static_cast<BufferSize>(sizeof(T)),
            .usageFlags = vk::BufferUsageFlagBits::eUniformBuffer,
            .updatable = info.updatable
        });
        return ConstantBuffer<T>(
            uniformBufferIndex++,
            reinterpret_cast<T*>(uniformBuffers.back().getMapped()),
            uniformBuffers.back().getSize(),
            info.updatable
        );
    }

    Texture texture(const TextureInfo& info);

    template <typename T>
    Sampler<T> sampler(const SamplerInfo& info) {
        samplers.emplace_back(device, vulkan::SamplerInfo{
            .dimensions = info.dimensions,
            .channels = info.channels,
            .format = info.format,
            .stride = sizeof(T)
        });
        return Sampler<T>(
            samplerIndex++,
            reinterpret_cast<T*>(samplers.back().getStagingMapped()),
            samplers.back().getStagingSize()
        );
    }

    AccelerationStructure accelerationStructure(const AccelerationStructureBuilder& builder);

    template <typename T>
    void update(const ConstantBuffer<T>& constantBuffer) {
        if (!uniformBuffers[constantBuffer.index].isUpdatable()) {
            throw std::runtime_error("Cannot update constant buffer that is set to non-updatable.");
        }

        constantBufferUpdates.insert(constantBuffer.index);
    }

    void wait() const;

    [[nodiscard]] const vulkan::Surface& getSurface() const noexcept { return surface; }
    [[nodiscard]] const vulkan::Device& getDevice() const noexcept { return device; }
    [[nodiscard]] const vulkan::Swapchain& getSwapchain() const noexcept { return swapchain; }

private:
    friend class Renderer;

    vulkan::Surface surface; // TODO: Make optional.
    vulkan::Device device;
    vulkan::Swapchain swapchain; // TODO: Make optional.
    vulkan::ShaderBindingTables shaderBindingTables = nullptr;

    RenderTarget renderTarget;

    // Resources:

    // Storage buffers.
    size_t storageBufferIndex = 0;
    std::vector<vulkan::StagedBuffer> storageBuffers;

    // Uniform buffers.
    size_t uniformBufferIndex = 0;
    std::vector<vulkan::StagedBuffer> uniformBuffers;

    // Images.
    size_t imageIndex = 0;
    std::vector<vulkan::Image> images;

    // Samplers.
    size_t samplerIndex = 0;
    std::vector<vulkan::Sampler> samplers;

    // Acceleration structures.
    bool hasProcedural = false;
    vulkan::StagedBuffer aabbBuffer = nullptr;

    std::vector<vulkan::BottomLevelAccelerationStructure> blas;
    vulkan::InstancesBuffer instancesBuffer = nullptr;
    vulkan::TopLevelAccelerationStructure tlas = nullptr;

    // Resources to update before rendering the next frame.
    std::unordered_set<size_t> constantBufferUpdates;

    template <typename T>
    vulkan::StagedBuffer& get(const StructuredBuffer<T>& resource) const {
        return storageBuffers[resource.index];
    }

    template <typename T>
    vulkan::StagedBuffer& get(const ConstantBuffer<T>& resource) const {
        return uniformBuffers[resource.index];
    }

    vulkan::Image& get(const Texture& resource);
    vulkan::TopLevelAccelerationStructure& get(const AccelerationStructure&);

    const vulkan::Descriptor& get(const Resource& resource) const;

    void createShaderBindingTables(const vulkan::Pipeline& pipeline, const vulkan::ShaderBindingTablesInfo& info);

    void initialize(const vk::raii::CommandBuffer& commandBuffer);
    void unstage();
    void restage(const vk::raii::CommandBuffer& commandBuffer);
};

}
