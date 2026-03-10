#include <engine/engine.hpp>
#include <engine/vulkan/vulkan.hpp>

namespace engine {

Context::Context(const ContextInfo& info) :
    instance(vulkan::InstanceInfo{
        .appName = info.appName.c_str(),
        .requiredExtensions = info.requiredExtensions,
        .enableValidationLayers = info.enableValidation
    }) {}

}
