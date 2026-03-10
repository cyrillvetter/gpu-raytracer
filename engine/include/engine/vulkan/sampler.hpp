#pragma once

#include <engine/engine.hpp>
#include <engine/vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <cstddef>

namespace engine::vulkan {

struct SamplerInfo final {
    Dimensions dimensions;
    int channels = 1;
    vk::Format format;
    BufferSize stride;
};

class Sampler final : public Descriptor {
public:
    NON_COPYABLE(Sampler)
    Sampler(Sampler&& other) noexcept;
    Sampler& operator=(Sampler&& other) noexcept;

    Sampler() = delete;
    Sampler(std::nullptr_t) {}

    explicit Sampler(const Device& device, const SamplerInfo& info);

    void writeSet(vk::WriteDescriptorSet& writer) const override {
        writer.setImageInfo(descriptorImageInfo);
    }

    vk::ImageMemoryBarrier2 transitionImageLayout(const ImageTransitionInfo& transitionImageLayout) const;
    void stage(const vk::raii::CommandBuffer& commandBuffer) const;
    void unstage();

    [[nodiscard]] void* getStagingMapped() const noexcept { return stagingBuffer.getMapped(); }
    [[nodiscard]] BufferSize getStagingSize() const noexcept { return stagingBuffer.getSize(); }

private:
    bool staged = true;

    vulkan::InitializedBuffer stagingBuffer = nullptr;
    vulkan::Image image = nullptr;

    vk::raii::Sampler sampler = nullptr;
    vk::DescriptorImageInfo descriptorImageInfo{};
};

}
