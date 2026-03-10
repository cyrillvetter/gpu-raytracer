#pragma once

#include <engine/engine.hpp>
#include <engine/vulkan/vulkan.hpp>

#include <vector>

namespace engine {

struct ContextInfo final {
    const std::string& appName;
    bool enableValidation = true;
    std::vector<const char*> requiredExtensions;
};

class Context final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(Context)
    Context() = delete;

    explicit Context(const ContextInfo& info);

    [[nodiscard]] const vulkan::Instance& getInstance() const noexcept { return instance; }

private:
    vulkan::Instance instance;
};

}
