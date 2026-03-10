#pragma once

#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <cstddef>

namespace engine::vulkan {

struct ImageInfo final {
    Dimensions dimensions;
    vk::Format format;
    vk::ImageUsageFlags usageFlags;
};

struct ImageTransitionInfo final {
    vk::ImageLayout oldLayout;
    vk::ImageLayout newLayout;

    vk::PipelineStageFlags2 srcStageMask;
    vk::AccessFlags2 srcAccessMask;
    vk::PipelineStageFlags2 dstStageMask;
    vk::AccessFlags2 dstAccessMask;
};

class Swapchain;
struct SwapchainAcquisition;

class Image final : public Descriptor {
public:
    NON_COPYABLE(Image)
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    Image() = delete;
    Image(std::nullptr_t) {}

    explicit Image(const Device& device, const ImageInfo& info);

    vk::ImageMemoryBarrier2 transitionImageLayout(const ImageTransitionInfo& transitionInfo) const;
    void blitToSwapchain(
        const Swapchain& swapchain,
        const vk::raii::CommandBuffer& commandBuffer,
        const SwapchainAcquisition& swapchainAcquisition
    ) const;
    void copyToSwapchain(
        const Swapchain& swapchain,
        const vk::raii::CommandBuffer& commandBuffer,
        const SwapchainAcquisition& swapchainAcquisition
    ) const;

    void writeSet(vk::WriteDescriptorSet& writer) const override {
        writer.setImageInfo(descriptorImageInfo);
    }

    [[nodiscard]] uint32_t getWidth() const noexcept { return width; }
    [[nodiscard]] uint32_t getHeight() const noexcept { return height; }

    [[nodiscard]] const vk::raii::Image& get() const noexcept { return image; }
    [[nodiscard]] const vk::raii::ImageView& getView() const noexcept { return imageView; }

private:
    uint32_t queueFamilyIndex;
    uint32_t width;
    uint32_t height;

    vk::raii::Image image = nullptr;
    vk::raii::ImageView imageView = nullptr;
    vk::raii::DeviceMemory memory = nullptr;

    vk::DescriptorImageInfo descriptorImageInfo{};

    void insertPreTransferBarriers(
        const Swapchain& swapchain,
        const vk::raii::CommandBuffer& commandBuffer,
        const SwapchainAcquisition& swapchainAcquisition
    ) const;

    void insertPostTransferBarriers(
        const Swapchain& swapchain,
        const vk::raii::CommandBuffer& commandBuffer,
        const SwapchainAcquisition& swapchainAcquisition,
        vk::PipelineStageFlagBits2 srcStageMask
    ) const;
};

}
