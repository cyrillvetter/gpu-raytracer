#include <engine/engine.hpp>
#include <stdexcept>
#include <string>
#include <optional>

namespace engine {

ShaderStage::ShaderStage(Shader shader, std::string entryPoint)
    : shader(shader), entryPoint(entryPoint) {}

TriangleHitGroup::TriangleHitGroup(
    std::optional<ShaderStage> closestHit,
    std::optional<ShaderStage> anyHit
) : closestHit(closestHit), anyHit(anyHit) {}

ProceduralHitGroup::ProceduralHitGroup(
    ShaderStage intersection,
    std::optional<ShaderStage> closestHit,
    std::optional<ShaderStage> anyHit
) : intersection(intersection), closestHit(closestHit), anyHit(anyHit) {}

ShaderStages& ShaderStages::addRaygen(const ShaderStage& raygenStage) {
    if (hasRaygen) {
        throw std::runtime_error("Only one raygen shader stage is supported.");
    }

    hasRaygen = true;

    std::array<Stage, 3> shaderStages = {
        Stage{ raygenStage.shader.filename, raygenStage.entryPoint, vk::ShaderStageFlagBits::eRaygenKHR }
    };
    groups.emplace_back(GroupType::Raygen, shaderStages, 1);
    shaders.insert(raygenStage.shader.filename);

    return *this;
}

ShaderStages& ShaderStages::addMiss(const ShaderStage& missStage) noexcept {
    std::array<Stage, 3> shaderStages = {
        Stage{ missStage.shader.filename, missStage.entryPoint, vk::ShaderStageFlagBits::eMissKHR }
    };
    groups.emplace_back(GroupType::Miss, shaderStages, 1);
    shaders.insert(missStage.shader.filename);
    return *this;
}

ShaderStages& ShaderStages::addCallable(const ShaderStage& callableStage) noexcept {
    std::array<Stage, 3> shaderStages = {
        Stage{ callableStage.shader.filename, callableStage.entryPoint, vk::ShaderStageFlagBits::eMissKHR }
    };
    groups.emplace_back(GroupType::Miss, shaderStages, 1);
    shaders.insert(callableStage.shader.filename);
    return *this;
}

ShaderStages& ShaderStages::addGroup(const TriangleHitGroup& triangleHitGroup) noexcept {
    std::array<Stage, 3> shaderStages;
    int i = 0;

    if (auto stage = triangleHitGroup.closestHit) {
        shaders.insert(stage->shader.filename);
        shaderStages[i].filename = stage->shader.filename;
        shaderStages[i].entryPoint = stage->entryPoint;
        shaderStages[i].stageFlag = vk::ShaderStageFlagBits::eClosestHitKHR;
        ++i;
    }
    if (auto stage = triangleHitGroup.anyHit) {
        shaders.insert(stage->shader.filename);
        shaderStages[i].filename = stage->shader.filename;
        shaderStages[i].entryPoint = stage->entryPoint;
        shaderStages[i].stageFlag = vk::ShaderStageFlagBits::eAnyHitKHR;
        ++i;
    }
    groups.emplace_back(GroupType::Triangle, shaderStages, i);
    return *this;
}

ShaderStages& ShaderStages::addGroup(const ProceduralHitGroup& proceduralHitGroup) noexcept {
    std::array<Stage, 3> shaderStages = {
        Stage{ proceduralHitGroup.intersection.shader.filename, proceduralHitGroup.intersection.entryPoint, vk::ShaderStageFlagBits::eIntersectionKHR }
    };
    int i = 1;

    if (auto stage = proceduralHitGroup.closestHit) {
        shaders.insert(stage->shader.filename);
        shaderStages[i].filename = stage->shader.filename;
        shaderStages[i].entryPoint = stage->entryPoint;
        shaderStages[i].stageFlag = vk::ShaderStageFlagBits::eClosestHitKHR;
        ++i;
    }
    if (auto stage = proceduralHitGroup.anyHit) {
        shaders.insert(stage->shader.filename);
        shaderStages[i].filename = stage->shader.filename;
        shaderStages[i].entryPoint = stage->entryPoint;
        shaderStages[i].stageFlag = vk::ShaderStageFlagBits::eAnyHitKHR;
        ++i;
    }
    groups.emplace_back(GroupType::Procedural, shaderStages, i);
    return *this;
}

}
