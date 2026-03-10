#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <optional>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <stdint.h>

#define REQUIRE_NONE vk::QueueFlags(~0)
#define EXCLUDE_NONE vk::QueueFlags(0)

static const std::vector requiredDeviceExtensions = {
    // General extensions.
    vk::KHRSwapchainExtensionName,
    vk::KHRSpirv14ExtensionName,
    vk::KHRSynchronization2ExtensionName,
    vk::EXTDescriptorIndexingExtensionName,
    vk::EXTScalarBlockLayoutExtensionName, // Relax data alignment.
    vk::KHRMaintenance1ExtensionName, // Allows fence in present function.

    // Ray tracing extensions.
    vk::KHRAccelerationStructureExtensionName,
    vk::KHRRayTracingPipelineExtensionName,
    vk::KHRDeferredHostOperationsExtensionName
};

// Used for scoring the available devices, the larger the penalty (result of this function),
// the less likely it is for the device to be picked.
// If the result is none, the device has missing requirements.
static std::optional<uint32_t> getPhysicalDevicePenalty(const vk::raii::PhysicalDevice& device) noexcept {
    auto properties = device.getProperties2().properties;

    // Supports Vulkan 1.4 and above.
    bool supportsVulkan14 = properties.apiVersion >= VK_API_VERSION_1_4;
    if (!supportsVulkan14) return {};

    // Check if any queue supports compute operations (needed for ray tracing operations) and graphics (for image blit operations).
    auto queueFamilies = device.getQueueFamilyProperties();
    bool supportsComputeGraphics = std::ranges::any_of(queueFamilies, [](const auto& qfp) {
        return !!(qfp.queueFlags & (vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eGraphics));
    });
    if (!supportsComputeGraphics) return {};

    // Check if all required device extensions are available.
    auto availableDeviceExtensions = device.enumerateDeviceExtensionProperties();
    bool supportsAllRequiredExtensions = true;

    for (const auto& requiredExtension : requiredDeviceExtensions) {
        bool deviceHasExtension = std::ranges::any_of(availableDeviceExtensions, [requiredExtension](auto const& availableDeviceExtension) {
            return strcmp(availableDeviceExtension.extensionName, requiredExtension) == 0;
        });

        if (!deviceHasExtension) {
            supportsAllRequiredExtensions = false;
        }
    }

    if (!supportsAllRequiredExtensions) return {};

    // Check if all required device features are available.
    const auto features = device.template getFeatures2<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceScalarBlockLayoutFeatures,
        vk::PhysicalDeviceBufferAddressFeaturesEXT,
        vk::PhysicalDeviceDescriptorIndexingFeaturesEXT,
        vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
        vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>();
    bool supportsRequiredFeatures =
        // General.
        features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&

        // Data layout.
        features.template get<vk::PhysicalDeviceScalarBlockLayoutFeatures>().scalarBlockLayout &&

        // Descriptor indexing.
        features.template get<vk::PhysicalDeviceDescriptorIndexingFeaturesEXT>().shaderSampledImageArrayNonUniformIndexing &&
        features.template get<vk::PhysicalDeviceDescriptorIndexingFeaturesEXT>().shaderStorageBufferArrayNonUniformIndexing &&
        features.template get<vk::PhysicalDeviceDescriptorIndexingFeaturesEXT>().runtimeDescriptorArray &&

        // Ray tracing.
        features.template get<vk::PhysicalDeviceBufferAddressFeaturesEXT>().bufferDeviceAddress &&
        features.template get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>().accelerationStructure &&
        features.template get<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>().rayTracingPipeline;

    if (!supportsRequiredFeatures) return {};

    switch (properties.deviceType) {
        // Prefer discrete gpus over all other types.
        case vk::PhysicalDeviceType::eDiscreteGpu: return 0;

        // Usually only available inside a virtual environment.
        case vk::PhysicalDeviceType::eVirtualGpu: return 1;

        // Integrated gpus are unlikely to have ray tracing support,
        // but should still be prefered over cpu processing.
        case vk::PhysicalDeviceType::eIntegratedGpu: return 2;

        // As a last resort, use cpu rendering. This will usually be LLVMpipe: https://docs.mesa3d.org/drivers/llvmpipe.html
        case vk::PhysicalDeviceType::eCpu: return 3;
        case vk::PhysicalDeviceType::eOther: return 4;
    }

    return {};
}

namespace engine::vulkan {

// TODO: Return exactly what is missing, if no device with full version/extension/feature support is available.

Device::Device(const Instance& instance, const Surface& surface) {
    auto physicalDevices = instance.get().enumeratePhysicalDevices();

    if (physicalDevices.empty()) {
        throw std::runtime_error("No GPU with Vulkan support found.");
    }

    bool foundDevice = false;
    uint32_t minPenalty = UINT32_MAX;

    for (const auto& device : physicalDevices) {
        const std::optional<uint32_t> penalty = getPhysicalDevicePenalty(device);
        if (penalty.has_value() && *penalty < minPenalty) {
            foundDevice = true;
            minPenalty = *penalty;

            physicalDevice = std::move(device);
            properties = physicalDevice.getProperties2().properties;
        }
    }

    if (!foundDevice) {
        throw std::runtime_error("No physical device with ray tracing support found");
    }

    const vk::StructureChain<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceScalarBlockLayoutFeatures,
        vk::PhysicalDeviceDescriptorIndexingFeaturesEXT,
        vk::PhysicalDeviceBufferDeviceAddressFeatures,
        vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
        vk::PhysicalDeviceRayTracingPipelineFeaturesKHR> featureChain = {
        // General.
        {}, // vk::PhysicalDeviceFeatures2
        { .synchronization2 = true },

        // Data layout.
        { .scalarBlockLayout = true },

        // Descriptor indexing.
        {
            .shaderSampledImageArrayNonUniformIndexing = true,
            .shaderStorageBufferArrayNonUniformIndexing = true,
            .runtimeDescriptorArray = true
        },

        // Ray tracing.
        { .bufferDeviceAddress = true },
        { .accelerationStructure = true },
        { .rayTracingPipeline = true }
    };

    // Queue usage requirements:
    //   - Compute queue -> ray tracing.
    //   - Graphics queue -> image blit.
    const auto queueProperties = physicalDevice.getQueueFamilyProperties();
    queueFamilyIndex = findQueueIndex(surface, queueProperties, {
        { vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer, EXCLUDE_NONE }
    }, true);

    float queuePriority = 0.0f;
    const vk::DeviceQueueCreateInfo queueCreateInfo{
        .queueFamilyIndex = queueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };

    const vk::DeviceCreateInfo deviceCreateInfo{
        .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
        .ppEnabledExtensionNames = requiredDeviceExtensions.data()
    };

    device = vk::raii::Device(physicalDevice, deviceCreateInfo);
    queue = vk::raii::Queue(device, queueFamilyIndex, 0);
}

void Device::waitForDevice() const {
    device.waitIdle();
}

uint32_t Device::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const {
    const vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type.");
}

uint32_t Device::findQueueIndex(
    const Surface& surface,
    const std::vector<vk::QueueFamilyProperties>& queueFamilies,
    const std::initializer_list<QueueFlagPair> flagPairs,
    const bool supportsPresentation
) {
    for (auto const& pair : flagPairs) {
        auto result = tryFindQueueIndex(surface, queueFamilies, pair, supportsPresentation);
        if (result.has_value()) {
            return *result;
        }
    }

    throw std::runtime_error("Could not find suitable queue.");
}

std::optional<uint32_t> Device::tryFindQueueIndex(
    const Surface& surface,
    const std::vector<vk::QueueFamilyProperties>& queueFamilies,
    const QueueFlagPair flagPair,
    const bool supportsPresentation
) {
    for (uint32_t qfpIndex = 0; qfpIndex < queueFamilies.size(); qfpIndex++) {
        auto queueFamily = queueFamilies[qfpIndex];

        bool correctFlags = queueFamily.queueFlags & flagPair.requiredFlags &&
            !(queueFamily.queueFlags & flagPair.excludedFlags);
        bool presentationSupport = !supportsPresentation || physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface.get());

        if (correctFlags && presentationSupport) {
            return qfpIndex;
        }
    }

    return {};
}

}
