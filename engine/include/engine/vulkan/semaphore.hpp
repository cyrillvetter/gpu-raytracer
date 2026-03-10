#pragma once

#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace engine::vulkan {

class Semaphore final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(Semaphore)
    Semaphore() = delete;

    explicit Semaphore(const Device& device);

    [[nodiscard]] const vk::raii::Semaphore& get() const noexcept { return semaphore; }

private:
    vk::raii::Semaphore semaphore;
};

}
