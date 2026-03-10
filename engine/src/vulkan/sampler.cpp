#include <engine/engine.hpp>
#include <engine/vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <stdexcept>

namespace engine::vulkan {

Sampler::Sampler(const Device& device, const SamplerInfo& info)
    : stagingBuffer(device, InitializedBufferInfo{
          .stride = info.stride,
          .count = static_cast<uint64_t>(info.dimensions.width * info.dimensions.height * info.channels),
          .usageFlags = vk::BufferUsageFlagBits::eTransferSrc
      }),
      image(device, ImageInfo{
          .dimensions = info.dimensions,
          .format = info.format,
          .usageFlags = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled
      }),
      // TODO: Make some of these settings visible to the abstraction layer.
      sampler(device.get(), vk::SamplerCreateInfo{
          .magFilter = vk::Filter::eLinear,
          .minFilter = vk::Filter::eLinear,
          .mipmapMode = vk::SamplerMipmapMode::eLinear,
          .addressModeU = vk::SamplerAddressMode::eRepeat,
          .addressModeV = vk::SamplerAddressMode::eRepeat,
          .addressModeW = vk::SamplerAddressMode::eRepeat,
          .mipLodBias = 0.0f,
          .anisotropyEnable = vk::False, // TODO: Enable if supported by device (also make configurable).
          .maxAnisotropy = 1.0f,
          .compareEnable = vk::False,
          .compareOp = vk::CompareOp::eAlways,
          .minLod = 0.0f,
          .maxLod = 0.0f,
          .unnormalizedCoordinates = vk::False,
      }),
      descriptorImageInfo({
          .sampler = *sampler,
          .imageView = *image.getView(),
          .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
      }) {}

vk::ImageMemoryBarrier2 Sampler::transitionImageLayout(const ImageTransitionInfo& transitionInfo) const {
    return image.transitionImageLayout(transitionInfo);
}

void Sampler::stage(const vk::raii::CommandBuffer& commandBuffer) const {
    if (!staged)
        throw std::runtime_error("Staging buffer was unstaged previously.");

    stagingBuffer.copyToImage(commandBuffer, image);
}

void Sampler::unstage() {
    staged = false;
    stagingBuffer.unmap();
    stagingBuffer = nullptr;
}

Sampler::Sampler(Sampler&& other) noexcept
    : stagingBuffer(std::move(other.stagingBuffer)),
      image(std::move(other.image)),
      sampler(std::move(other.sampler)),
      descriptorImageInfo(std::move(other.descriptorImageInfo)) {
    descriptorImageInfo.setSampler(sampler);
    descriptorImageInfo.setImageView(image.getView());
}

Sampler& Sampler::operator=(Sampler&& other) noexcept {
    if (this != &other) {
        stagingBuffer = std::move(other.stagingBuffer);
        image = std::move(other.image);
        sampler = std::move(other.sampler);

        descriptorImageInfo = std::move(other.descriptorImageInfo);
        descriptorImageInfo.setSampler(sampler);
        descriptorImageInfo.setImageView(image.getView());
    }
    return *this;
}

}
