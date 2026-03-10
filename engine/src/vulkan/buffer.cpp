#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

constexpr vk::BufferUsageFlags getAdditionalDeviceFlags(bool updatable) {
    return updatable ? vk::BufferUsageFlagBits::eTransferDst : vk::BufferUsageFlags{};
}

namespace engine::vulkan {

Buffer::Buffer(const Device& device, const BufferInfo& info)
    : stride(info.stride), count(info.count), size(stride * count) {
    // Create buffer.
    uint32_t queueFamilyIndex = device.getQueueFamilyIndex();
    vk::BufferCreateInfo bufferCreateInfo{
        .size = size,
        .usage = info.usageFlags | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        .sharingMode = vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &queueFamilyIndex
    };
    buffer = vk::raii::Buffer(device.get(), bufferCreateInfo);

    // Allocate memory.
    vk::MemoryRequirements requirements = buffer.getMemoryRequirements();
    uint32_t memoryTypeIndex = device.findMemoryType(requirements.memoryTypeBits, info.memoryFlags);
    vk::MemoryAllocateFlagsInfo allocFlagsInfo{
        .flags = vk::MemoryAllocateFlagBits::eDeviceAddress
    };
    vk::MemoryAllocateInfo memoryInfo{
        .pNext = &allocFlagsInfo,
        .allocationSize = requirements.size,
        .memoryTypeIndex = memoryTypeIndex,
    };
    memory = vk::raii::DeviceMemory(device.get(), memoryInfo);
    buffer.bindMemory(*memory, 0);

    // Get buffer device address:
    deviceAddress = device.get().getBufferAddress(vk::BufferDeviceAddressInfoKHR{
        .buffer = *buffer
    });

    descriptorInfo.setBuffer(buffer);
    descriptorInfo.setOffset(0);
    descriptorInfo.setRange(size);
}

Buffer::Buffer(Buffer&& other) noexcept
    : buffer(std::move(other.buffer)),
      memory(std::move(other.memory)),
      stride(std::exchange(other.stride, 0)),
      count(std::exchange(other.count, 0)),
      size(std::exchange(other.size, 0)),
      deviceAddress(std::exchange(other.deviceAddress, ~0)),
      descriptorInfo(std::move(other.descriptorInfo)) {
    descriptorInfo.setBuffer(buffer);
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        buffer = std::move(other.buffer);
        memory = std::move(other.memory);
        stride = std::exchange(other.stride, 0);
        count = std::exchange(other.count, 0);
        size = std::exchange(other.size, 0);
        deviceAddress = std::exchange(other.deviceAddress, 0);

        descriptorInfo = std::move(other.descriptorInfo);
        descriptorInfo.setBuffer(buffer);
    }
    return *this;
}

UninitializedBuffer::UninitializedBuffer(const Device& device, const UninitializedBufferInfo& info) :
    Buffer(device, BufferInfo{
        .stride = info.stride,
        .count = info.count,
        .usageFlags = info.usageFlags,
        .memoryFlags = info.memoryFlags
    }) {}

InitializedBuffer::InitializedBuffer(const Device& device, const InitializedBufferInfo& info) :
    Buffer(device, BufferInfo{
        .stride = info.stride,
        .count = info.count,
        .usageFlags = info.usageFlags,
        .memoryFlags =
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent
    }) {
    mapped = memory.mapMemory(0, size);
}

InitializedBuffer::InitializedBuffer(const Device& device, const InitializedBufferInfo& info, const void* data) :
    InitializedBuffer(device, info) {
    memcpy(mapped, data, size);
}

void InitializedBuffer::unmap() noexcept {
    if (mapped) {
        memory.unmapMemory();
        mapped = nullptr;
    }
}

void InitializedBuffer::copyToBuffer(const vk::raii::CommandBuffer& commandBuffer, const UninitializedBuffer& dstBuffer) const {
    commandBuffer.copyBuffer(buffer, dstBuffer.get(), vk::BufferCopy(0, 0, size));
}

void InitializedBuffer::copyToImage(const vk::raii::CommandBuffer& commandBuffer, const Image& image) const {
    const vk::BufferImageCopy bufferImageCopy{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { image.getWidth(), image.getHeight(), 1 }
    };
    commandBuffer.copyBufferToImage(buffer, image.get(), vk::ImageLayout::eTransferDstOptimal, bufferImageCopy);
}

StagedBuffer::StagedBuffer(const Device& device, const StagedBufferInfo& info) :
    UninitializedBuffer(device, UninitializedBufferInfo{
        .stride = info.stride,
        .count = info.count,
        .usageFlags = info.usageFlags | vk::BufferUsageFlagBits::eTransferDst,
        .memoryFlags = vk::MemoryPropertyFlagBits::eDeviceLocal,
    }),
    updatable(info.updatable),
    stagingBuffer(InitializedBuffer(device, InitializedBufferInfo{
        .stride = info.stride,
        .count = info.count,
        .usageFlags = vk::BufferUsageFlagBits::eTransferSrc
    })) {}

StagedBuffer::StagedBuffer(const Device& device, const StagedBufferInfo& info, const void* data) :
    UninitializedBuffer(device, UninitializedBufferInfo{
        .stride = info.stride,
        .count = info.count,
        .usageFlags = info.usageFlags | vk::BufferUsageFlagBits::eTransferDst,
        .memoryFlags = vk::MemoryPropertyFlagBits::eDeviceLocal
    }),
    updatable(info.updatable),
    stagingBuffer(InitializedBuffer(device, InitializedBufferInfo{
        .stride = info.stride,
        .count = info.count,
        .usageFlags = vk::BufferUsageFlagBits::eTransferSrc
    }, data)) {}

void StagedBuffer::stage(const vk::raii::CommandBuffer& commandBuffer) const {
    if (!staged)
        throw std::runtime_error("Staging buffer was unstaged previously.");

    stagingBuffer.copyToBuffer(commandBuffer, *this);
}

void StagedBuffer::unstage() {
    if (!updatable) {
        staged = false;
        stagingBuffer.unmap();
        stagingBuffer = nullptr;
    }
}

}
