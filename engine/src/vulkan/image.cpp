#include <engine/engine.hpp>
#include <engine/vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

static constexpr vk::ImageSubresourceRange defaultSubresourceRange{
    .aspectMask = vk::ImageAspectFlagBits::eColor,
    .baseMipLevel = 0,
    .levelCount = 1,
    .baseArrayLayer = 0,
    .layerCount = 1
};

namespace engine::vulkan {

Image::Image(const Device& device, const ImageInfo& info)
    : queueFamilyIndex(device.getQueueFamilyIndex()),
      width(static_cast<uint32_t>(info.dimensions.width)),
      height(static_cast<uint32_t>(info.dimensions.height)) {
    // Image creation.
    uint32_t queueFamilyIndex = device.getQueueFamilyIndex();
    const vk::ImageCreateInfo imageInfo{
        .imageType = vk::ImageType::e2D,
        .format = info.format,
        .extent = { width, height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .usage = info.usageFlags,
        .sharingMode = vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &queueFamilyIndex,
        .initialLayout = vk::ImageLayout::eUndefined,
    };
    image = vk::raii::Image(device.get(), imageInfo);

    // Memory allocation.
    vk::MemoryRequirements memoryRequirements = image.getMemoryRequirements();
    uint32_t memoryTypeIndex = device.findMemoryType(memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    const vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = memoryTypeIndex
    };
    memory = vk::raii::DeviceMemory(device.get(), allocInfo);

    // Bind memory to image.
    image.bindMemory(*memory, 0);

    // Create image view.
    const vk::ImageViewCreateInfo imageViewInfo{
        .image = *image,
        .viewType = vk::ImageViewType::e2D,
        .format = info.format,
        .subresourceRange = defaultSubresourceRange
    };
    imageView = vk::raii::ImageView(device.get(), imageViewInfo);

    // Update descriptor set info:
    descriptorImageInfo.setImageView(*imageView);
    descriptorImageInfo.setImageLayout(vk::ImageLayout::eGeneral);
}

vk::ImageMemoryBarrier2 Image::transitionImageLayout(const ImageTransitionInfo& transitionInfo) const {
    return vk::ImageMemoryBarrier2{
        .srcStageMask = transitionInfo.srcStageMask,
        .srcAccessMask = transitionInfo.srcAccessMask,
        .dstStageMask = transitionInfo.dstStageMask,
        .dstAccessMask = transitionInfo.dstAccessMask,
        .oldLayout = transitionInfo.oldLayout,
        .newLayout = transitionInfo.newLayout,
        .srcQueueFamilyIndex = queueFamilyIndex,
        .dstQueueFamilyIndex = queueFamilyIndex,
        .image = image,
        .subresourceRange = defaultSubresourceRange
    };
}

void Image::blitToSwapchain(
    const Swapchain& swapchain,
    const vk::raii::CommandBuffer& commandBuffer,
    const SwapchainAcquisition& swapchainAcquisition
) const {
    insertPreTransferBarriers(swapchain, commandBuffer, swapchainAcquisition);

    // Blit image to swapchain:
    auto swapchainExtent = swapchain.getExtent();
    const vk::ImageBlit2 blit{
        .srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
        .srcOffsets = std::array<vk::Offset3D, 2> {
            vk::Offset3D{ 0, 0, 0 },
            vk::Offset3D{ static_cast<int32_t>(width), static_cast<int32_t>(height), 1 },
        },
        .dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
        .dstOffsets = std::array<vk::Offset3D, 2> {
            vk::Offset3D{ 0, 0, 0 },
            vk::Offset3D{ static_cast<int32_t>(swapchainExtent.width), static_cast<int32_t>(swapchainExtent.height), 1 },
        }
    };
    commandBuffer.blitImage2(vk::BlitImageInfo2{
        .srcImage = image,
        .srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
        .dstImage = swapchainAcquisition.image,
        .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
        .regionCount = 1,
        .pRegions = &blit,
        .filter = vk::Filter::eLinear
    });

    insertPostTransferBarriers(swapchain, commandBuffer, swapchainAcquisition, vk::PipelineStageFlagBits2::eBlit);
}

void Image::copyToSwapchain(
    const Swapchain& swapchain,
    const vk::raii::CommandBuffer& commandBuffer,
    const SwapchainAcquisition& swapchainAcquisition
) const {
    insertPreTransferBarriers(swapchain, commandBuffer, swapchainAcquisition);

    // Copy image to swapchain, this will not apply conversion to srgb. The image will appear dark on the screen.
    auto swapchainExtent = swapchain.getExtent();
    const vk::ImageCopy imageCopy{
        .srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
        .dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
        .extent = vk::Extent3D(swapchainExtent.width, swapchainExtent.height, 1),
    };
    commandBuffer.copyImage(
        image,
        vk::ImageLayout::eTransferSrcOptimal,
        swapchainAcquisition.image,
        vk::ImageLayout::eTransferDstOptimal,
        imageCopy
    );

    insertPostTransferBarriers(swapchain, commandBuffer, swapchainAcquisition, vk::PipelineStageFlagBits2::eBlit);
}

void Image::insertPreTransferBarriers(
    const Swapchain& swapchain,
    const vk::raii::CommandBuffer& commandBuffer,
    const SwapchainAcquisition& swapchainAcquisition
) const {
    // Prepare images for transfer.
    std::array<vk::ImageMemoryBarrier2, 2> preBlitBarriers = {
        transitionImageLayout(ImageTransitionInfo{
            .oldLayout = vk::ImageLayout::eGeneral,
            .newLayout = vk::ImageLayout::eTransferSrcOptimal,
            .srcStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
            .srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
            .dstAccessMask = vk::AccessFlagBits2::eTransferRead
        }),
        swapchain.transitionSwapchainLayout(swapchainAcquisition, ImageTransitionInfo{
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eTransferDstOptimal,
            .srcStageMask = vk::PipelineStageFlagBits2::eNone,
            .srcAccessMask = vk::AccessFlagBits2::eNone,
            .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
            .dstAccessMask = vk::AccessFlagBits2::eTransferWrite
        })
    };
    commandBuffer.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = preBlitBarriers.size(),
        .pImageMemoryBarriers = preBlitBarriers.data()
    });
}

void Image::insertPostTransferBarriers(
    const Swapchain& swapchain,
    const vk::raii::CommandBuffer& commandBuffer,
    const SwapchainAcquisition& swapchainAcquisition,
    vk::PipelineStageFlagBits2 srcStageMask
) const {
    // Transition images to presentable/general states.
    std::array<vk::ImageMemoryBarrier2, 2> postBlitBarriers = {
        transitionImageLayout(ImageTransitionInfo{
            .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
            .newLayout = vk::ImageLayout::eGeneral,
            .srcStageMask = srcStageMask,
            .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eNone,
            .dstAccessMask = vk::AccessFlagBits2::eNone
        }),
        swapchain.transitionSwapchainLayout(swapchainAcquisition, ImageTransitionInfo{
            .oldLayout = vk::ImageLayout::eTransferDstOptimal,
            .newLayout = vk::ImageLayout::ePresentSrcKHR,
            .srcStageMask = srcStageMask,
            .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eNone,
            .dstAccessMask = vk::AccessFlagBits2::eNone
        })
    };
    commandBuffer.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = postBlitBarriers.size(),
        .pImageMemoryBarriers = postBlitBarriers.data()
    });
}

Image::Image(Image&& other) noexcept
    : width(std::exchange(other.width, 0)),
      height(std::exchange(other.height, 0)),
      image(std::move(other.image)),
      imageView(std::move(other.imageView)),
      memory(std::move(other.memory)),
      descriptorImageInfo(std::move(other.descriptorImageInfo)) {
    descriptorImageInfo.setImageView(*imageView);
}

Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
        width = std::exchange(other.width, 0);
        height = std::exchange(other.height, 0);
        image = std::move(other.image);
        imageView = std::move(other.imageView);
        memory = std::move(other.memory);

        descriptorImageInfo = std::move(other.descriptorImageInfo);
        descriptorImageInfo.setImageView(*imageView);
    }
    return *this;
}

}
