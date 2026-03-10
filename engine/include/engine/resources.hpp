#pragma once

#include <engine/engine.hpp>
#include <concepts>
#include <stdexcept>

namespace engine {

struct Resource {
public:
    Resource() = delete;

    bool operator==(const Resource& other) const noexcept {
        return index == other.index && type == other.type;
    }

protected:
    friend class Device;
    friend class ShaderResources;
    friend struct std::hash<Resource>;

    size_t index;
    vk::DescriptorType type;

    explicit Resource(vk::DescriptorType type, size_t index);
};

}

namespace std {

template<> struct hash<engine::Resource> {
    size_t operator()(const engine::Resource& resource) const noexcept {
        size_t indexHash = hash<uint32_t>{}(resource.index);
        size_t typeHash = hash<uint32_t>{}(static_cast<uint32_t>(resource.type));
        return indexHash ^ (typeHash << 1);
    }
};

}

namespace engine {

struct WritableResourceBase : public Resource {
public:
    virtual ~WritableResourceBase() = default;

protected:
    friend class Device;
    friend class ShaderResources;

    virtual void onExpose() = 0;
    explicit WritableResourceBase(vk::DescriptorType type, size_t index) : Resource(type, index) {}
};

template <typename T>
struct WritableResource : public WritableResourceBase {
public:
    WritableResource() = delete;

    void fill(const T* data) {
        if (!isMapped) {
            throw std::runtime_error("Trying to write into an unmapped buffer.");
        }

        memcpy(mapped, data, static_cast<size_t>(size));
    }

    T* data() const {
        if (!this->isMapped) {
            throw std::runtime_error("Trying to write into an unmapped buffer.");
        }

        return this->mapped;
    }

    T& operator[](int index) {
        if (!this->mapped) {
            throw std::runtime_error("Trying to write into an unmapped structured buffer.");
        }

        return this->mapped[index];
    }

protected:
    T* mapped;
    bool isMapped = true;
    BufferSize size;
    bool updatable;

    void onExpose() override {
        if (!updatable) {
            mapped = nullptr;
            isMapped = false;
        }
    }

    explicit WritableResource(vk::DescriptorType type, size_t index, T* data, BufferSize size, bool updatable)
        : WritableResourceBase(type, index), mapped(data), size(size), updatable(updatable) {}
};

struct StructuredBufferInfo final {
    bool accelerationStructureInput = false;
};

template <typename T>
struct StructuredBuffer final : public WritableResource<T> {
public:
    StructuredBuffer() = default;

private:
    friend class Device;
    friend class AccelerationStructureBuilder;
    friend class ShaderResources;

    explicit StructuredBuffer(size_t index, T* data, BufferSize size)
        : WritableResource<T>(vk::DescriptorType::eStorageBuffer, index, data, size, false) {}
};

struct ConstantBufferInfo final {
    bool updatable = false;
};

template <typename T>
struct ConstantBuffer final : public WritableResource<T> {
private:
    friend class Device;
    friend class ShaderResources;

    explicit ConstantBuffer(size_t index, T* data, BufferSize size, bool updatable)
        : WritableResource<T>(vk::DescriptorType::eUniformBuffer, index, data, size, updatable) {}
};

template <typename T>
struct PushConstant final : public Resource {
public:
    explicit PushConstant() : Resource(vk::DescriptorType{}, 0), size(sizeof(T)) {}

private:
    friend class ShaderResources;
    friend class Renderer;

    size_t size;
    uint32_t offset = 0;
    vk::ShaderStageFlags stageFlags{};
};

typedef vk::Format Format;

struct TextureInfo final {
    Dimensions dimensions;
    Format format = Format::eB8G8R8A8Unorm;

    // TODO: Remove the usageFlags member.
    vk::ImageUsageFlags usageFlags =
        vk::ImageUsageFlagBits::eStorage |
        vk::ImageUsageFlagBits::eTransferSrc;
};

struct Texture final : public Resource {
private:
    friend class Device;
    friend class ShaderResources;

    explicit Texture(size_t index) : Resource(vk::DescriptorType::eStorageImage, index) {}
};

struct SamplerInfo final {
    Dimensions dimensions;
    int channels = 1;
    Format format = Format::eB8G8R8A8Srgb;
};

template <typename T>
struct Sampler final : public WritableResource<T> {
private:
    friend class Device;
    friend class ShaderResources;

    explicit Sampler(size_t index, T* data, BufferSize size)
        : WritableResource<T>(vk::DescriptorType::eCombinedImageSampler, index, data, size, false) {}
};

struct AccelerationStructure final : public Resource {
private:
    friend class Device;
    friend class ShaderResources;

    explicit AccelerationStructure() : Resource(vk::DescriptorType::eAccelerationStructureKHR, 0) {}
};

enum class ShaderStageFlags : uint32_t {
    Raygen = static_cast<uint32_t>(vk::ShaderStageFlagBits::eRaygenKHR),
    AnyHit = static_cast<uint32_t>(vk::ShaderStageFlagBits::eAnyHitKHR),
    ClosestHit = static_cast<uint32_t>(vk::ShaderStageFlagBits::eClosestHitKHR),
    Miss = static_cast<uint32_t>(vk::ShaderStageFlagBits::eMissKHR),
    Intersection = static_cast<uint32_t>(vk::ShaderStageFlagBits::eIntersectionKHR),
    Callable = static_cast<uint32_t>(vk::ShaderStageFlagBits::eCallableKHR),
    All = static_cast<uint32_t>(vk::ShaderStageFlagBits::eAll),
};

FLAG_OPERATORS(ShaderStageFlags)

static constexpr vk::ShaderStageFlags convertShaderStageFlags(ShaderStageFlags flags) {
    uint32_t temp = static_cast<uint32_t>(flags);
    return static_cast<vk::ShaderStageFlags>(temp);
}

class ShaderResources final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(ShaderResources)
    ShaderResources() = default;

    template <typename T>
    ShaderResources& expose(PushConstant<T>& pushConstant, ShaderStageFlags stageFlags) noexcept {
        const vk::ShaderStageFlags vkStageFlags = convertShaderStageFlags(stageFlags);
        pushConstantSetBuilder.add(sizeof(T), vkStageFlags);
        pushConstant.offset = pushConstantSetBuilder.getCurrentOffset();
        pushConstant.stageFlags = vkStageFlags;
        return *this;
    }

    ShaderResources& expose(WritableResourceBase& writableResource, ShaderStageFlags stageFlags) noexcept;

    ShaderResources& expose(const Resource& resource, ShaderStageFlags stageFlags) noexcept;

    template <typename T>
    inline ShaderResources& expose(
        std::vector<T>& resources,
        ShaderStageFlags stageFlags
    ) noexcept requires std::derived_from<T, WritableResourceBase> {
        return expose(std::span{ resources }, stageFlags);
    }

    template <typename T>
    ShaderResources& expose(
        std::span<T> resources,
        ShaderStageFlags stageFlags
    ) noexcept requires std::derived_from<T, WritableResourceBase> {
        if (!resources.empty()) {
            for (auto& resource : resources) {
                resource.onExpose();
            }

            descriptors.emplace_back(
                std::vector<Resource>(resources.begin(), resources.end()),
                resources.front().type,
                convertShaderStageFlags(stageFlags)
            );
        }
        return *this;
    }

    template <typename T>
    inline ShaderResources& expose(
        const std::vector<T>& resources,
        ShaderStageFlags stageFlags
    ) noexcept requires std::derived_from<T, Resource> {
        return expose(std::span{ resources }, stageFlags);
    }

    template <typename T>
    ShaderResources& expose(
        std::span<const T> resources,
        ShaderStageFlags stageFlags
    ) noexcept requires std::derived_from<T, Resource> {
        if (!resources.empty()) {
            descriptors.emplace_back(
                std::vector<Resource>(resources.begin(), resources.end()),
                resources.front().type,
                convertShaderStageFlags(stageFlags)
            );
        }
        return *this;
    }

private:
    friend class Renderer;

    struct Descriptor final {
        std::vector<Resource> resources;
        vk::DescriptorType type;
        vk::ShaderStageFlags stageFlags;
    };

    std::vector<Descriptor> descriptors;
    vulkan::PushConstantSetBuilder pushConstantSetBuilder;
};

}
