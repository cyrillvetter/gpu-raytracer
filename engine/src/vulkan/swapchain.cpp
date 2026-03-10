#include <engine/engine.hpp>
#include <engine/vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <span>
#include <initializer_list>
#include <algorithm>
#include <stdexcept>

namespace engine::vulkan {

Swapchain::Swapchain(const Device& device, const Surface& surface, const SwapchainInfo& info)
    : queueFamilyIndex(device.getQueueFamilyIndex()),
      width(info.width),
      height(info.height),
      vsync(info.vsync),
      srgb(info.srgb),
      extent({ info.width, info.height }) {
    createSwapchain(device, surface);
}

void Swapchain::createSwapchain(const Device& device, const Surface& surface) {
    auto physicalDevice = device.getPhysical();

    auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface.get());
    surfaceFormat = chooseSurfaceFormat(device.getPhysical().getSurfaceFormatsKHR(*surface.get()));
    uint32_t queueFamilyIndex = device.getQueueFamilyIndex();

    const vk::SwapchainCreateInfoKHR createInfo{
        .surface = *surface.get(),
        .minImageCount = chooseMinImageCount(surfaceCapabilities),
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eTransferDst,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &queueFamilyIndex,
        .preTransform = surfaceCapabilities.currentTransform,
        .presentMode = choosePresentMode(physicalDevice.getSurfacePresentModesKHR(*surface.get())),
        .clipped = true,
    };

    swapchain = vk::raii::SwapchainKHR(device.get(), createInfo);
    images = swapchain.getImages();
}

void Swapchain::cleanup() noexcept {
    swapchain = nullptr;
}

void Swapchain::recreate(const Device& device, const Surface& surface, uint32_t width, uint32_t height) {
    this->width = width;
    this->height = height;

    cleanup();
    createSwapchain(device, surface);
}

SwapchainAcquisition Swapchain::acquireNextImage(const Semaphore& semaphore) const {
    auto result = swapchain.acquireNextImage(UINT64_MAX, semaphore.get());
    if (!result.has_value()) {
        throw std::runtime_error("Swapchain image acquisition failed");
    }

    uint32_t imageIndex = result.value;
    return SwapchainAcquisition(imageIndex, images[imageIndex], *this);
}

vk::ImageMemoryBarrier2 Swapchain::transitionSwapchainLayout(
    const SwapchainAcquisition& swapchainAcquisition,
    const ImageTransitionInfo& transitionInfo
) const {
    return vk::ImageMemoryBarrier2{
        .srcStageMask = transitionInfo.srcStageMask,
        .srcAccessMask = transitionInfo.srcAccessMask,
        .dstStageMask = transitionInfo.dstStageMask,
        .dstAccessMask = transitionInfo.dstAccessMask,
        .oldLayout = transitionInfo.oldLayout,
        .newLayout = transitionInfo.newLayout,
        .srcQueueFamilyIndex = queueFamilyIndex,
        .dstQueueFamilyIndex = queueFamilyIndex,
        .image = swapchainAcquisition.image,
        .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 },
    };
}

void Swapchain::present(
    const Device& device,
    const SwapchainAcquisition& swapchainAcquisition,
    std::span<const vk::Semaphore> semaphores
) const {
    auto result = device.getQueue().presentKHR(vk::PresentInfoKHR{
        .waitSemaphoreCount = static_cast<uint32_t>(semaphores.size()),
        .pWaitSemaphores = semaphores.data(),
        .swapchainCount = 1,
        .pSwapchains = &*swapchain,
        .pImageIndices = &swapchainAcquisition.imageIndex
    });
    if (result != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to present image.");
    }
}

vk::PresentModeKHR Swapchain::choosePresentMode(std::span<const vk::PresentModeKHR> availablePresentModes) {
    if (vsync) {
        // NOTE: FIFO is required to be supported by any vulkan implementation: https://docs.vulkan.org/refpages/latest/refpages/source/VkPresentModeKHR.html
        return vk::PresentModeKHR::eFifo;
    }

    std::initializer_list<vk::PresentModeKHR> presentModes = {
        vk::PresentModeKHR::eMailbox,
        vk::PresentModeKHR::eImmediate
    };

    for (vk::PresentModeKHR mode : presentModes) {
        bool supports = std::ranges::any_of(availablePresentModes, [mode](const vk::PresentModeKHR value) {
            return value == mode;
        });

        if (supports) {
            return mode;
        }
    }

    // Fallback if the other present modes are not supported.
    return vk::PresentModeKHR::eFifo;
}

vk::SurfaceFormatKHR Swapchain::chooseSurfaceFormat(std::span<const vk::SurfaceFormatKHR> availableFormats) {
    assert(!availableFormats.empty());
    vk::SurfaceFormatKHR best = availableFormats[0];

    for (const auto& supported : availableFormats) {
        if (!srgb && supported.format == vk::Format::eB8G8R8A8Unorm) {
            return supported;
        }

        if (supported.format == vk::Format::eB8G8R8A8Srgb && supported.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            best = supported;
        }
    }

    return best;
}

uint32_t Swapchain::chooseMinImageCount(const vk::SurfaceCapabilitiesKHR& capabilities) {
    auto minImageCount = std::max(3u, capabilities.minImageCount);
    if ((0 < capabilities.maxImageCount) && (capabilities.maxImageCount < minImageCount)) {
        minImageCount = capabilities.maxImageCount;
    }

    return minImageCount;
}

}
