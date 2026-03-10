#pragma once

#include <engine/engine.hpp>
#include <engine/vulkan/vulkan.hpp>

#include <vulkan/vulkan_raii.hpp>
#include <cstddef>

namespace engine::vulkan {

typedef uint64_t BufferAddress;

class Buffer : public Descriptor {
public:
    NON_COPYABLE(Buffer)
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    Buffer() = delete;
    Buffer(std::nullptr_t) {}

    void writeSet(vk::WriteDescriptorSet& writer) const override {
        writer.setPBufferInfo(&descriptorInfo);
    }

    [[nodiscard]] const vk::raii::Buffer& get() const noexcept { return buffer; }
    [[nodiscard]] BufferSize getSize() const noexcept { return size; }
    [[nodiscard]] BufferSize getStride() const noexcept { return stride; }
    [[nodiscard]] BufferSize getCount() const noexcept { return count; }
    [[nodiscard]] BufferAddress getAddress() const noexcept { return deviceAddress; }

protected:
    vk::raii::Buffer buffer = nullptr;
    vk::raii::DeviceMemory memory = nullptr;

    BufferSize stride;
    BufferSize count;
    BufferSize size;
    BufferAddress deviceAddress;
    vk::DescriptorBufferInfo descriptorInfo;

    struct BufferInfo final {
        size_t stride;
        size_t count;
        vk::BufferUsageFlags usageFlags;
        vk::MemoryPropertyFlags memoryFlags;
    };

    explicit Buffer(const Device& device, const BufferInfo& info);
};

struct UninitializedBufferInfo final {
    BufferSize stride;
    BufferSize count = 1;
    vk::BufferUsageFlags usageFlags;
    vk::MemoryPropertyFlags memoryFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;
};

class UninitializedBuffer : public Buffer {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(UninitializedBuffer)
    UninitializedBuffer() = delete;
    UninitializedBuffer(std::nullptr_t) : Buffer(nullptr) {}

    explicit UninitializedBuffer(const Device& device, const UninitializedBufferInfo& info);
};

struct InitializedBufferInfo final {
    BufferSize stride;
    BufferSize count = 1;
    vk::BufferUsageFlags usageFlags;
};

class StagedBuffer;
class Image;

class InitializedBuffer : public Buffer {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(InitializedBuffer)
    InitializedBuffer() = delete;
    InitializedBuffer(std::nullptr_t) : Buffer(nullptr) {}

    explicit InitializedBuffer(const Device& device, const InitializedBufferInfo& info);
    explicit InitializedBuffer(const Device& device, const InitializedBufferInfo& info, const void* data);

    [[nodiscard]] void* getMapped() const noexcept { return mapped; }

    void unmap() noexcept;
    void copyToBuffer(const vk::raii::CommandBuffer& commandBuffer, const UninitializedBuffer& dstBuffer) const;
    void copyToImage(const vk::raii::CommandBuffer& commandBuffer, const Image& image) const;

private:
    friend class StagedBuffer;

    void* mapped = nullptr;
    size_t count;
};

struct StagedBufferInfo final {
    BufferSize stride;
    BufferSize count = 1;
    vk::BufferUsageFlags usageFlags;
    bool updatable = false;
};

class StagedBuffer : public UninitializedBuffer {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(StagedBuffer)

    StagedBuffer() = delete;
    StagedBuffer(std::nullptr_t) : UninitializedBuffer(nullptr) {}

    explicit StagedBuffer(const Device& device, const StagedBufferInfo& info);
    explicit StagedBuffer(const Device& device, const StagedBufferInfo& info, const void* data);

    [[nodiscard]] bool isUpdatable() const noexcept { return updatable; }
    [[nodiscard]] void* getMapped() const noexcept { return stagingBuffer.getMapped(); }

    void stage(const vk::raii::CommandBuffer& commandBuffer) const;
    void unstage();

private:
    bool updatable;
    bool staged = true;
    InitializedBuffer stagingBuffer = nullptr;
};

}
