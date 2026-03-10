#pragma once

#include <engine/engine.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>
#include <variant>

namespace engine {

struct TriangleAccelerationStructure final {
    Resource vertexBuffer;
    Resource indexBuffer;
};

struct AABB {
    glm::vec3 min;
    glm::vec3 max;
};

struct AccelerationStructureInstanceInfo final {
    uint32_t instanceId = 0;
    glm::mat4x3 transform = glm::mat4x3(1.0);
    uint32_t hitGroupIndex = 0;
};

class AccelerationStructureBuilder final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(AccelerationStructureBuilder)
    explicit AccelerationStructureBuilder();

    template <typename VT, typename IT>
    AccelerationStructureBuilder& add(
        StructuredBuffer<VT> vertexBuffer,
        StructuredBuffer<IT> indexBuffer,
        const AccelerationStructureInstanceInfo& info = {}
    ) {
        structures.push_back(TriangleAccelerationStructure{
            .vertexBuffer = vertexBuffer,
            .indexBuffer = indexBuffer
        });
        triangleStructureCount++;

        addInstance(info);
        blasIndex++;
        return *this;
    }

    AccelerationStructureBuilder& add(AABB aabb, const AccelerationStructureInstanceInfo& info);

private:
    friend class Device;

    // Acceleration structures to be built.

    size_t blasIndex = 0;
    std::vector<std::variant<TriangleAccelerationStructure, AABB>> structures;

    // Acceleration structure type counts.
    size_t triangleStructureCount = 0;
    size_t aabbStructureCount = 0;

    // Top level instances of the acceleration structures.
    struct InstanceInfo final {
        uint32_t structureId;
        size_t blasIndex;
        glm::mat4x3 transform;
        uint32_t hitShaderGroupIndex;
    };

    std::vector<InstanceInfo> instanceInfos;

    void addInstance(const AccelerationStructureInstanceInfo& info);
};

}
