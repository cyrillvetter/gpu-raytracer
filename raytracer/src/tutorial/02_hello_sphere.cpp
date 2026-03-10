#include <engine/engine.hpp>
#include "../util/window.hpp"
#include <glm/glm.hpp>

const int WIDTH = 1500;
const int HEIGHT = 1000;
const std::string NAME = "02 - Hello Spere";

struct Vertex {
    glm::vec4 position;
};
const std::vector<Vertex> vertices = {
    {{ 0.0f, 0.75f, 0.0f, 0.0f }},
    {{ -0.75f, -0.75f, 0.0f, 0.0f }},
    {{ 0.75, -0.75f, 0.0f, 0.0f }}
};
const std::vector<uint32_t> indices = { 0, 1, 2 };

struct Sphere {
    glm::vec3 center;
    float radius;

    engine::AABB aabb() const {
        return engine::AABB{ glm::vec3(-radius), glm::vec3(radius) };
    }

    glm::mat4x3 mat() const {
        glm::mat4x3 transform(1.0f);
        transform[3] = center;
        return transform;
    }
};

int main() {
    Window window(WindowInfo{
        .width = WIDTH,
        .height = HEIGHT,
        .name = NAME
    });

    const engine::Context context(engine::ContextInfo{
        .appName = NAME,
        .enableValidation = true,
        .requiredExtensions = window.getRequiredExtensions()
    });

    const Dimensions dimensions(WIDTH, HEIGHT);
    engine::Device device(context, engine::RenderTarget{
        .surface = window.createSurface(context),
        .dimensions = dimensions,
        .vsync = true
    });

    auto vertexBuffer = device.structuredBuffer<Vertex>(vertices.size(), { .accelerationStructureInput = true });
    vertexBuffer.fill(vertices.data());
    auto indexBuffer = device.structuredBuffer<uint32_t>(indices.size(), { .accelerationStructureInput = true });
    indexBuffer.fill(indices.data());

    std::array<Sphere, 2> spheres = {
        Sphere{ glm::vec3(1.0, 0.0, 0.0), 0.3, },
        Sphere{ glm::vec3(-1.0, 0.0, 0.0), 0.3, },
    };
    auto sphereBuffer = device.structuredBuffer<Sphere>(spheres.size());
    sphereBuffer.fill(spheres.data());

    engine::AccelerationStructureBuilder asBuilder;
    asBuilder.add(vertexBuffer, indexBuffer, { .hitGroupIndex = 0 });
    for (uint32_t i = 0; i < spheres.size(); ++i)
        asBuilder.add(spheres[i].aabb(), { .instanceId = i, .transform = spheres[i].mat(), .hitGroupIndex = 1 });
    auto accelerationStructure = device.accelerationStructure(asBuilder);

    auto outputTexture = device.texture(engine::TextureInfo{ .dimensions = dimensions });
    engine::ShaderResources shaderResources;
    shaderResources
        .expose(accelerationStructure, engine::ShaderStageFlags::Raygen)
        .expose(sphereBuffer, engine::ShaderStageFlags::Intersection)
        .expose(outputTexture, engine::ShaderStageFlags::Raygen);

    engine::Shader shader("shaders/tutorial/02_hello_sphere.spv");
    engine::ShaderStages shaderStages;
    shaderStages
        .addRaygen(engine::ShaderStage(shader, "raygenMain"))
        .addGroup(engine::TriangleHitGroup(engine::ShaderStage(shader, "closestHitMain")))
        .addGroup(engine::ProceduralHitGroup(engine::ShaderStage(shader, "sphereIntersect"), engine::ShaderStage(shader, "sphereClosest")))
        .addMiss(engine::ShaderStage(shader, "missMain"));

    engine::Renderer renderer(device, shaderResources, shaderStages);

    while (!window.shouldClose()) {
        window.pollEvents();
        renderer.render(device, outputTexture);
    }

    device.wait();
}
