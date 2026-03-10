#pragma once

#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <vector>
#include <optional>
#include <initializer_list>
#include <stdint.h>

namespace engine::vulkan {

class Swapchain;

class Device final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(Device)
    Device() = delete;

    explicit Device(const Instance& instance, const Surface& surface);

    [[nodiscard]] const vk::raii::Device& get() const noexcept { return device; }
    [[nodiscard]] const vk::raii::PhysicalDevice& getPhysical() const noexcept { return physicalDevice; }

    [[nodiscard]] const vk::raii::Queue& getQueue() const noexcept { return queue; }
    [[nodiscard]] uint32_t getQueueFamilyIndex() const noexcept { return queueFamilyIndex; }

    [[nodiscard]] const vk::PhysicalDeviceProperties getProperties() const noexcept { return properties; }

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

    void waitForDevice() const;

private:
    vk::raii::PhysicalDevice physicalDevice = nullptr;
    vk::PhysicalDeviceProperties properties;
    vk::raii::Device device = nullptr;

    vk::raii::Queue queue = nullptr;
    uint32_t queueFamilyIndex = ~0;

    // Queue search utility.

    struct QueueFlagPair final {
        vk::QueueFlags requiredFlags;
        vk::QueueFlags excludedFlags;
    };

    uint32_t findQueueIndex(
        const Surface& surface,
        const std::vector<vk::QueueFamilyProperties>& queueFamilies,
        const std::initializer_list<QueueFlagPair> flagPairs,
        const bool supportsPresentation
    );

    std::optional<uint32_t> tryFindQueueIndex(
        const Surface& surface,
        const std::vector<vk::QueueFamilyProperties>& queueFamilies,
        const QueueFlagPair flagPair,
        const bool supportsPresentation
    );
};

}
