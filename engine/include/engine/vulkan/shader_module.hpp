#pragma once

#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <string_view>
#include <filesystem>
#include <cstddef>

namespace engine::vulkan {

class ShaderModule final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(ShaderModule)
    ShaderModule() = delete;
    ShaderModule(std::nullptr_t) {}

    explicit ShaderModule(const Device& device, std::string_view filename);

    const vk::raii::ShaderModule& get() const noexcept { return module; }

private:
    vk::raii::ShaderModule module = nullptr;
    std::vector<char> readFile(const std::filesystem::path& path);
};

}
