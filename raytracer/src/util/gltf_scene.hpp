#pragma once

#include <glm/glm.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>

#include <vector>
#include <string_view>

struct Camera final {
    float meterPerPixel;
    float focalLength;
    glm::vec4 origin;
    glm::mat4 rotation;

    Camera() = default;
    Camera(const fastgltf::Asset& asset, float width, float height);
};

enum class MaterialType : int {
    Diffuse = 0,
    Specular = 1,
    Metal = 2,
    Glass = 3,
    Emissive = 4
};
struct Material final {
    glm::vec3 color;
    float roughness;

    Material() = delete;
    Material(const fastgltf::Material& material);
};

struct Vertex final {
    glm::vec4 position; // NOTE: MUST be the first element inside the struct.
    glm::vec4 normal;
};

struct Scene;

struct Mesh final {
    glm::mat4x3 transform;

    int materialIndex;
    MaterialType materialType;

    size_t vertexCount;
    size_t indexCount;

    void copyVertices(const Scene& scene, Vertex* vertexBuffer) const;
    void copyIndices(const Scene& scene, uint32_t* indexBuffer) const;

    Mesh() = delete;
    Mesh(const fastgltf::Asset& asset, const fastgltf::Node& meshNode, size_t primitiveIndex);

private:
    size_t meshIndex;
    size_t primitiveIndex;
};

struct Scene final {
    fastgltf::Asset asset;

    Camera camera;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;

    Scene() = delete;
    Scene(std::string_view path, size_t width, size_t height);

private:
    MaterialType getMaterialType(int materialIndex) const;
};
