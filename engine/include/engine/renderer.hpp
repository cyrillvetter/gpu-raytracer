#pragma once

#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <unordered_map>

namespace engine {

struct RenderInfo final {
    int maxRecursionDepth = 1;
};

class Renderer final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(Renderer)
    Renderer() = delete;

    explicit Renderer(
        Device& device,
        const ShaderResources& shaderResources,
        const ShaderStages& shaderStages,
        const RenderInfo& info = {}
    );

    void render(Device& device, const Texture& output);

    template <typename T>
    void pushConstant(const PushConstant<T>& pushConstant, const T* data) {
        commandBuffers
            .at(activeComputeBufferIndex)
            .pushConstants2(vk::PushConstantsInfo{
                .layout = pipeline.getLayout(),
                .stageFlags = pushConstant.stageFlags,
                .offset = pushConstant.offset,
                .size = sizeof(T),
                .pValues = data
            });
    }

private:
    vulkan::CommandPool commandPool;
    vulkan::CommandBuffers commandBuffers = nullptr;

    vulkan::DescriptorPool descriptorPool = nullptr;
    vulkan::DescriptorSet descriptorSet = nullptr;

    vulkan::PushConstantSet pushConstantSet;

    std::unordered_map<std::string, vulkan::ShaderModule> shaderModules;
    vulkan::Pipeline pipeline = nullptr;

    uint32_t semaphoreIndex = 0;
    std::vector<vulkan::Semaphore> presentCompleteSemaphores;
    std::vector<vulkan::Semaphore> renderFinishedSemaphores;
    vulkan::Fence synchronizationFence;

    bool initialized = false;
    bool cleanedUp = false;

    vulkan::SwapchainAcquisition swapchainAcquisition = nullptr;
    size_t activeComputeBufferIndex = 0;

    void initialize(Device& device, const vk::raii::CommandBuffer& commandBuffer);
    void beginRecording(const Device& device);

    void setupDescriptors(const Device& device, const ShaderResources& shaderResources);
    void setupPipeline(Device& device, const ShaderStages& shaderStages, const RenderInfo& info);
    void setupSemaphores(const Device& device);
};

}
