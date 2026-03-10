#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <algorithm>
#include <stdexcept>
#include <cstring>

namespace engine::vulkan {

// TODO: Throw the names of unsupported validation layers and instance extensions.

Instance::Instance(const InstanceInfo& info) : context() {
    vk::ApplicationInfo appInfo{
        .pApplicationName = info.appName,
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0), // TODO: Expose applicationVersion in the InstanceInfo struct.
        .pEngineName = "engine",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = vk::ApiVersion14
    };

    // Setup validation layers.
    std::vector<const char*> requiredValidationLayers;
    if (info.enableValidationLayers) {
        requiredValidationLayers.assign(info.validationLayers.begin(), info.validationLayers.end());
    }

    // Check if any validation layer is not supported.
    auto layerProperties = context.enumerateInstanceLayerProperties();
    bool anyUnsupportedValidationLayer = std::ranges::any_of(requiredValidationLayers, [&layerProperties](auto const& requiredLayer) {
        return std::ranges::none_of(layerProperties, [requiredLayer](auto const& layerProperty) {
            return strcmp(layerProperty.layerName, requiredLayer) == 0;
        });
    });

    if (anyUnsupportedValidationLayer) {
        throw std::runtime_error("One or more validation layers are not supported.");
    }

    // Check if any instance extension is not supported.
    auto extensionProperties = context.enumerateInstanceExtensionProperties();
    bool anyUnsupportedExtension = std::ranges::any_of(info.requiredExtensions, [&extensionProperties](auto const& requiredExtension) {
        return std::ranges::none_of(extensionProperties, [requiredExtension](auto const& extensionProperty) {
            return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
        });
    });

    if (anyUnsupportedExtension) {
        throw std::runtime_error("One or more instance extensions are not supported.");
    }

    vk::InstanceCreateInfo createInfo{
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(requiredValidationLayers.size()),
        .ppEnabledLayerNames = requiredValidationLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(info.requiredExtensions.size()),
        .ppEnabledExtensionNames = info.requiredExtensions.data()
    };

    instance = vk::raii::Instance(context, createInfo);
}

}
