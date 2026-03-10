#include "gltf_scene.hpp"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <filesystem>
#include <stdexcept>
#include <string_view>
#include <variant>
#include <algorithm>

static constexpr auto supportedExtensions = fastgltf::Extensions::KHR_materials_transmission;

static glm::mat4x3 getTransform(const fastgltf::Node& node) {
    const auto& trs = std::get<fastgltf::TRS>(node.transform);

    glm::vec3 translation(trs.translation.x(), trs.translation.y(), -trs.translation.z());
    glm::quat quat(-trs.rotation.w(), trs.rotation.x(), trs.rotation.y(), -trs.rotation.z());
    glm::vec3 scale(trs.scale.x(), trs.scale.y(), trs.scale.z());

    glm::mat4 rotationScaleMatrix = glm::mat4_cast(quat);
    rotationScaleMatrix = glm::scale(rotationScaleMatrix, scale);

    glm::mat4x3 transformMatrix;
    transformMatrix[0] = glm::vec3(rotationScaleMatrix[0]);
    transformMatrix[1] = glm::vec3(rotationScaleMatrix[1]);
    transformMatrix[2] = glm::vec3(rotationScaleMatrix[2]);

    transformMatrix[3] = translation;

    return transformMatrix;
}

Camera::Camera(const fastgltf::Asset& asset, float width, float height) {
    auto camNode = std::ranges::find_if(asset.nodes, [&asset](const fastgltf::Node& n) {
        return n.cameraIndex.has_value() &&
            std::holds_alternative<fastgltf::Camera::Perspective>(asset.cameras[*n.cameraIndex].camera);
    });

    if (camNode == asset.nodes.end()) {
        throw std::runtime_error("No perspective camera defined in gltf file");
    }

    auto camera = std::get<fastgltf::Camera::Perspective>(asset.cameras[*camNode->cameraIndex].camera);
    float aspectRatio = camera.aspectRatio.value_or(width / height);
    float h = 1.0f / aspectRatio;

    meterPerPixel = h / height;
    focalLength = (h / 2.0f) / std::tan(camera.yfov / 2.0f);

    const auto& trs = std::get<fastgltf::TRS>(camNode->transform);
    origin = glm::vec4(trs.translation.x(), trs.translation.y(), -trs.translation.z(), 1.0f);

    glm::quat quat(-trs.rotation.w(), trs.rotation.x(), trs.rotation.y(), -trs.rotation.z());
    rotation = glm::mat4_cast(quat);
}

Material::Material(const fastgltf::Material& material) {
    // Roughness is not physically accurate in this renderer, remap to 0.5..1 for best results.
    roughness = 0.5 + 0.5 * material.pbrData.roughnessFactor;

    if (material.emissiveFactor.x() > 0.0f || material.emissiveFactor.y() > 0.0f || material.emissiveFactor.z() > 0.0f) {
        color = glm::vec3(material.emissiveFactor.x(), material.emissiveFactor.y(), material.emissiveFactor.z());
    } else {
        color = glm::vec3(material.pbrData.baseColorFactor.x(), material.pbrData.baseColorFactor.y(), material.pbrData.baseColorFactor.z());
    }
}

Mesh::Mesh(const fastgltf::Asset& asset, const fastgltf::Node& meshNode, size_t primitiveIndex)
    : meshIndex(meshNode.meshIndex.value()), primitiveIndex(primitiveIndex) {
    transform = getTransform(meshNode);

    const auto& primitive = asset.meshes[meshNode.meshIndex.value()].primitives[primitiveIndex];

    // Get position accessor.
    auto* positionIt = primitive.findAttribute("POSITION");
    auto& positionAccessor = asset.accessors[positionIt->accessorIndex];
    vertexCount = positionAccessor.count;

    auto& indexAccessor = asset.accessors[*primitive.indicesAccessor];
    indexCount = indexAccessor.count;

    // NOTE: Use the first material if no material is set.
    materialIndex = primitive.materialIndex.value_or(0);
}

void Mesh::copyVertices(const Scene& scene, Vertex* vertexBuffer) const {
    const auto& primitive = scene.asset.meshes[meshIndex].primitives[primitiveIndex];

    // TODO: Check if copyFromAccessor can be used here instead of iterating.

    auto* positionIt = primitive.findAttribute("POSITION");
    auto& positionAccessor = scene.asset.accessors[positionIt->accessorIndex];
    fastgltf::iterateAccessorWithIndex<glm::vec3>(
        scene.asset,
        positionAccessor,
        [&](glm::vec3 pos, size_t i) {
            vertexBuffer[i].position = glm::vec4(pos, 0.0);
            vertexBuffer[i].position.z *= -1.0f;
        }
    );

    auto* normalIt = primitive.findAttribute("NORMAL");
    auto& normalAccessor = scene.asset.accessors[normalIt->accessorIndex];
    fastgltf::iterateAccessorWithIndex<glm::vec3>(
        scene.asset,
        normalAccessor,
        [&](glm::vec3 normal, size_t i) {
            vertexBuffer[i].normal = glm::vec4(normal, 0.0);
            vertexBuffer[i].normal.z *= -1.0f;
        }
    );
}

void Mesh::copyIndices(const Scene& scene, uint32_t* indexBuffer) const {
    const auto& primitive = scene.asset.meshes[meshIndex].primitives[primitiveIndex];

    auto& indexAccessor = scene.asset.accessors[*primitive.indicesAccessor];
    fastgltf::copyFromAccessor<uint32_t>(
        scene.asset,
        indexAccessor,
        indexBuffer
    );
}

Scene::Scene(std::string_view path, size_t width, size_t height) {
    auto fsPath = std::filesystem::path(path);

    fastgltf::Parser parser(supportedExtensions);
    auto data = fastgltf::GltfDataBuffer::FromPath(fsPath);

    auto loadedAsset = parser.loadGltf(data.get(), fsPath.parent_path(), fastgltf::Options::DecomposeNodeMatrices);

    if (loadedAsset.error() != fastgltf::Error::None) {
        throw std::runtime_error("Failed to load gltf file.");
    }

    asset = std::move(loadedAsset.get());
    camera = Camera(asset, width, height);

    std::vector<MaterialType> materialTypes;
    materialTypes.reserve(asset.materials.size());

    for (int i = 0; i < asset.materials.size(); ++i) {
        materials.emplace_back(asset.materials[i]);
        materialTypes.push_back(getMaterialType(i));
    }

    for (const auto& node : asset.nodes) {
        if (!node.meshIndex.has_value())
            continue;

        const auto& mesh = asset.meshes[*node.meshIndex];
        for (size_t i = 0; i < mesh.primitives.size(); ++i) {
            Mesh mesh(asset, node, i);
            mesh.materialType = materialTypes[mesh.materialIndex];
            meshes.push_back(mesh);
        }
    }
}

MaterialType Scene::getMaterialType(int materialIndex) const {
    const auto& material = asset.materials[materialIndex];

    if (material.transmission && material.transmission->transmissionFactor > 0.0f) {
        return MaterialType::Glass;
    } else if (material.emissiveFactor.x() > 0.0f || material.emissiveFactor.y() > 0.0f || material.emissiveFactor.z() > 0.0f) {
        return MaterialType::Emissive;
    } else if (material.pbrData.metallicFactor >= 1.0f) {
        return MaterialType::Metal;
    } else if (material.pbrData.roughnessFactor >= 1.0f) {
        return MaterialType::Diffuse;
    } else {
        return MaterialType::Specular;
    }
}
