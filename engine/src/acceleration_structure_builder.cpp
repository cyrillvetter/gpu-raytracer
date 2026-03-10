#include <engine/engine.hpp>
#include <engine/vulkan/vulkan.hpp>

#include <glm/glm.hpp>

namespace engine {

AccelerationStructureBuilder::AccelerationStructureBuilder() {}

void AccelerationStructureBuilder::addInstance(const AccelerationStructureInstanceInfo& info) {
    instanceInfos.push_back(InstanceInfo{
        .structureId = info.instanceId,
        .blasIndex = blasIndex,
        .transform = info.transform,
        .hitShaderGroupIndex = info.hitGroupIndex
    });
}

AccelerationStructureBuilder& AccelerationStructureBuilder::add(AABB aabb, const AccelerationStructureInstanceInfo& info) {
    structures.push_back(aabb);
    aabbStructureCount++;
    addInstance(info);
    blasIndex++;
    return *this;
}

}
