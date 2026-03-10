#pragma once

#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vector>

namespace engine::vulkan {

struct InstanceInfo final {
    const char* appName = "raytracer";

    std::vector<const char*> requiredExtensions;

    bool enableValidationLayers = true;
    std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
};

class Instance final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(Instance)
    Instance() = delete;

    explicit Instance(const InstanceInfo& info);

    [[nodiscard]] const vk::raii::Instance& get() const noexcept { return instance; }

private:
    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
};

}
