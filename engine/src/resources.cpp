#include <engine/engine.hpp>

namespace engine {

Resource::Resource(vk::DescriptorType type, size_t index) : type(type), index(index) {}

ShaderResources& ShaderResources::expose(WritableResourceBase& writableResource, ShaderStageFlags stageFlags) noexcept {
    writableResource.onExpose();
    descriptors.emplace_back(std::vector<Resource> { writableResource }, writableResource.type, convertShaderStageFlags(stageFlags));
    return *this;
}

ShaderResources& ShaderResources::expose(const Resource& resource, ShaderStageFlags stageFlags) noexcept {
    descriptors.emplace_back(std::vector<Resource> { resource }, resource.type, convertShaderStageFlags(stageFlags));
    return *this;
}

}
