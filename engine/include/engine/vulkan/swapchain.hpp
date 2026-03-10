#pragma once

#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <vector>
#include <span>
#include <cstddef>

namespace engine::vulkan {

struct SwapchainInfo final {
    uint32_t width;
    uint32_t height;
    bool vsync = true;
    bool srgb = true;
};

struct SwapchainAcquisition final {
public:
    SwapchainAcquisition() = delete;
    SwapchainAcquisition(std::nullptr_t) {}

    uint32_t imageIndex;
    vk::Image image;

private:
    friend class Swapchain;
    SwapchainAcquisition(uint32_t imageIndex, vk::Image image, const Swapchain& swapchain)
        : imageIndex(imageIndex), image(image) {}
};

class Swapchain final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(Swapchain)
    Swapchain() = delete;

    explicit Swapchain(const Device& device, const Surface& surface, const SwapchainInfo& info);

    void cleanup() noexcept;
    void recreate(const Device& device, const Surface& surface, uint32_t width, uint32_t height);

    SwapchainAcquisition acquireNextImage(const Semaphore& semaphore) const;
    vk::ImageMemoryBarrier2 transitionSwapchainLayout(
        const SwapchainAcquisition& swapchainAcquisition,
        const ImageTransitionInfo& transitionInfo
    ) const;
    void present(
        const Device& device,
        const SwapchainAcquisition& swapchainAcquisition,
        std::span<const vk::Semaphore> semaphores
    ) const;

    [[nodiscard]] const vk::raii::SwapchainKHR& get() const noexcept { return swapchain; }
    [[nodiscard]] vk::Image getImage(uint32_t imageIndex) const { return images[imageIndex]; }
    [[nodiscard]] uint32_t getImageCount() const noexcept { return static_cast<uint32_t>(images.size()); }
    [[nodiscard]] vk::Extent2D getExtent() const noexcept { return extent; }

private:
    uint32_t queueFamilyIndex;

    uint32_t width;
    uint32_t height;
    bool vsync;
    bool srgb;

    vk::raii::SwapchainKHR swapchain = nullptr;
    std::vector<vk::Image> images;

    vk::SurfaceFormatKHR surfaceFormat;
    vk::Extent2D extent;

    void createSwapchain(const Device& device, const Surface& surface);

    vk::Extent2D chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities);
    vk::PresentModeKHR choosePresentMode(std::span<const vk::PresentModeKHR> availablePresentModes);
    vk::SurfaceFormatKHR chooseSurfaceFormat(std::span<const vk::SurfaceFormatKHR> availableFormats);

    static uint32_t chooseMinImageCount(const vk::SurfaceCapabilitiesKHR& capabilities);
};

}
