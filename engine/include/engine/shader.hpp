#pragma once

#include <engine/engine.hpp>
#include <unordered_set>
#include <string>
#include <optional>

namespace engine {

class Shader final {
public:
    Shader() = delete;
    explicit Shader(std::string filename) : filename(filename) {}

    std::string filename;
};

class ShaderStage final {
public:
    ShaderStage() = delete;
    explicit ShaderStage(Shader shader, std::string entryPoint);

    Shader shader;
    std::string entryPoint;
};

class TriangleHitGroup final {
public:
    TriangleHitGroup() = delete;
    explicit TriangleHitGroup(
        std::optional<ShaderStage> closestHit = {},
        std::optional<ShaderStage> anyHit = {}
    );

private:
    friend class ShaderStages;

    std::optional<ShaderStage> closestHit;
    std::optional<ShaderStage> anyHit;
};

class ProceduralHitGroup final {
public:
    ProceduralHitGroup() = delete;
    explicit ProceduralHitGroup(
        ShaderStage intersection,
        std::optional<ShaderStage> closestHit = {},
        std::optional<ShaderStage> anyHit = {}
    );

private:
    friend class ShaderStages;

    ShaderStage intersection;
    std::optional<ShaderStage> closestHit;
    std::optional<ShaderStage> anyHit;
};

class ShaderStages final {
public:
    NON_COPYABLE_DEFAULT_MOVABLE(ShaderStages)
    ShaderStages() = default;

    ShaderStages& addRaygen(const ShaderStage& raygenStage);
    ShaderStages& addMiss(const ShaderStage& missStage) noexcept;
    ShaderStages& addCallable(const ShaderStage& callableStage) noexcept;
    ShaderStages& addGroup(const TriangleHitGroup& triangleHitGroup) noexcept;
    ShaderStages& addGroup(const ProceduralHitGroup& proceduralHitGroup) noexcept;

private:
    friend class Renderer;

    enum class GroupType : size_t {
        Raygen,
        Triangle,
        Procedural,
        Miss,
        Callable
    };

    struct Stage final {
        std::string filename;
        std::string entryPoint;
        vk::ShaderStageFlagBits stageFlag;
    };

    struct Group final {
        GroupType groupType;
        std::array<Stage, 3> stages;
        size_t count;
    };

    std::unordered_set<std::string> shaders;
    std::vector<Group> groups;
    bool hasRaygen = false;
};

}
