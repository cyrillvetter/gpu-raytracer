#pragma once

#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace engine::vulkan {

class Fence final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(Fence)
    Fence() = delete;

    explicit Fence(const Device& device);

    void reset(const Device& device) const;
    void waitIndefinitely(const Device& device) const;

    [[nodiscard]] const vk::raii::Fence& get() const noexcept { return fence; }

private:
    vk::raii::Fence fence;
};

}
