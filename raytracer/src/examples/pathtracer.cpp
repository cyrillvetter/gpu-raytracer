#include <engine/engine.hpp>
#include "engine/resources.hpp"
#include "../util/window.hpp"
#include "../util/gltf_scene.hpp"

#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <chrono>
#include <random>

const int WIDTH = 1500;
const int HEIGHT = 1000;

struct PushConstant {
    uint32_t frame;
    uint32_t inc;
    float rand;
};

int main () {
    Window window(WindowInfo{
        .width = WIDTH,
        .height = HEIGHT,
        .name = "raytracer",
        .disableCursor = true
    });
    const Dimensions dimensions(WIDTH, HEIGHT);

    engine::Context context(engine::ContextInfo{
        .appName = "pathtracer",
        .enableValidation = false,
        .requiredExtensions = window.getRequiredExtensions()
    });

    engine::Device device(context, engine::RenderTarget{
        .surface = window.createSurface(context),
        .dimensions = dimensions,
        .vsync = false
    });

    auto outputTexture = device.texture(engine::TextureInfo{
        .dimensions = dimensions
    });
    auto accumulationTexture = device.texture(engine::TextureInfo{
        .dimensions = dimensions,
        .format = vk::Format::eR32G32B32A32Sfloat,
        .usageFlags = vk::ImageUsageFlagBits::eStorage
    });

    Scene scene("../assets/dragon.glb", WIDTH, HEIGHT);
    auto meshCount = scene.meshes.size();

    std::vector<engine::StructuredBuffer<Vertex>> vertexBuffers;
    vertexBuffers.reserve(meshCount);
    std::vector<engine::StructuredBuffer<uint32_t>> indexBuffers;
    vertexBuffers.reserve(meshCount);
    std::vector<uint32_t> materialIndices;
    materialIndices.reserve(meshCount);
    engine::AccelerationStructureBuilder asBuilder;

    for (uint32_t i = 0; i < scene.meshes.size(); ++i) {
        const auto& mesh = scene.meshes[i];
        vertexBuffers.push_back(device.structuredBuffer<Vertex>(
            mesh.vertexCount, {
            .accelerationStructureInput = true
        }));
        mesh.copyVertices(scene, vertexBuffers.back().data());
        indexBuffers.push_back(device.structuredBuffer<uint32_t>(
            mesh.indexCount, {
            .accelerationStructureInput = true
        }));
        mesh.copyIndices(scene, indexBuffers.back().data());
        materialIndices.push_back(mesh.materialIndex);
        asBuilder.add(vertexBuffers.back(), indexBuffers.back(), {
            .instanceId = i,
            .transform = mesh.transform,
            .hitGroupIndex = static_cast<uint32_t>(mesh.materialType)
        });
    }

    auto accelerationStructure = device.accelerationStructure(asBuilder);

    auto materialIndicesBuffer = device.structuredBuffer<uint32_t>(materialIndices.size());
    materialIndicesBuffer.fill(materialIndices.data());
    auto materialBuffer = device.structuredBuffer<Material>(scene.materials.size());
    materialBuffer.fill(scene.materials.data());
    auto cameraBuffer = device.constantBuffer<Camera>({ .updatable = true });
    cameraBuffer.fill(&scene.camera);

    engine::PushConstant<PushConstant> frameConstant;

    engine::ShaderResources shaderResources;
    shaderResources
        .expose(accelerationStructure, engine::ShaderStageFlags::Raygen | engine::ShaderStageFlags::ClosestHit)
        .expose(cameraBuffer, engine::ShaderStageFlags::Raygen)
        .expose(outputTexture, engine::ShaderStageFlags::Raygen)
        .expose(accumulationTexture, engine::ShaderStageFlags::Raygen)
        .expose(vertexBuffers, engine::ShaderStageFlags::ClosestHit)
        .expose(indexBuffers, engine::ShaderStageFlags::ClosestHit)
        .expose(materialIndicesBuffer, engine::ShaderStageFlags::ClosestHit)
        .expose(frameConstant, engine::ShaderStageFlags::Raygen | engine::ShaderStageFlags::ClosestHit)
        .expose(materialBuffer, engine::ShaderStageFlags::ClosestHit);

    engine::Shader shader("shaders/examples/pathtracer.spv");
    engine::ShaderStages shaderStages;
    shaderStages
        .addRaygen(engine::ShaderStage(shader, "raygenMain"))
        .addGroup(engine::TriangleHitGroup(engine::ShaderStage(shader, "specularClosestHitMain")))
        .addGroup(engine::TriangleHitGroup(engine::ShaderStage(shader, "specularClosestHitMain")))
        .addGroup(engine::TriangleHitGroup(engine::ShaderStage(shader, "reflectiveClosestHitMain")))
        .addGroup(engine::TriangleHitGroup(engine::ShaderStage(shader, "refractiveClosestHitMain")))
        .addGroup(engine::TriangleHitGroup(engine::ShaderStage(shader, "emissiveClosestHitMain")))
        .addMiss(engine::ShaderStage(shader, "missMain"));

    engine::Renderer renderer(device, shaderResources, shaderStages, engine::RenderInfo{
        .maxRecursionDepth = 1
    });

    auto lastTime = std::chrono::high_resolution_clock::now();
    std::default_random_engine generator;
    std::uniform_real_distribution<float> distribution(0.0, 1000.0);
    PushConstant pc{ 1, 1, distribution(generator) };

    glm::quat initialRotation = glm::quat_cast(scene.camera.rotation);

    const float SPEED = 1.3;

    while (!window.shouldClose()) {
        window.pollEvents();

        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> delta = currentTime - lastTime;
        float deltaTime = delta.count();
        lastTime = currentTime;

        glm::vec2 pos = window.getPos();

        glm::quat yaw = glm::angleAxis(pos.x, glm::vec3(0, 1, 0));
        glm::quat pitch = glm::angleAxis(pos.y, glm::vec3(1, 0, 0));
        glm::quat orientation = yaw * pitch;

        cameraBuffer.data()->rotation = glm::mat4_cast(initialRotation * orientation);

        glm::vec3 move = orientation * window.getInput();
        move *= deltaTime * SPEED;
        cameraBuffer.data()->origin.z += move.z;
        cameraBuffer.data()->origin.x += move.x;
        cameraBuffer.data()->origin.y += move.y;

        device.update(cameraBuffer);

        renderer.pushConstant(frameConstant, &pc);
        renderer.render(device, outputTexture);

        if (window.hasMoved()) {
            pc.frame = 1;
        } else {
            pc.frame++;
        }
        pc.inc++;
        pc.rand = distribution(generator);
    }

    device.wait();
}
