#include <engine/engine.hpp>

#include <vulkan/vulkan_raii.hpp>

#include <string_view>
#include <fstream>
#include <filesystem>

namespace engine::vulkan {

ShaderModule::ShaderModule(const Device& device, std::string_view filename) {
    std::filesystem::path file(filename);

    std::vector<char> shaderCode = readFile(file);
    vk::ShaderModuleCreateInfo shaderModuleCreateInfo{
        .codeSize = shaderCode.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t*>(shaderCode.data())
    };

    module = vk::raii::ShaderModule(device.get(), shaderModuleCreateInfo);
}

std::vector<char> ShaderModule::readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + path.string());
    }

    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
    return buffer;
}

}
