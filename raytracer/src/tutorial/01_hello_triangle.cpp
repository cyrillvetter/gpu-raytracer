#include <engine/engine.hpp>
#include "../util/window.hpp"
#include <glm/glm.hpp>

const int WIDTH = 1500;
const int HEIGHT = 1000;
const std::string NAME = "01 - Hello Triangle";

struct Vertex {
    glm::vec4 position;
};

const std::vector<Vertex> vertices = {
    {{ 0.0f, 0.75f, 0.0f, 0.0f }},
    {{ -0.75f, -0.75f, 0.0f, 0.0f }},
    {{ 0.75, -0.75f, 0.0f, 0.0f }}
};
const std::vector<uint32_t> indices = { 0, 1, 2 };

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

    engine::AccelerationStructureBuilder asBuilder;
    asBuilder.add(vertexBuffer, indexBuffer);
    auto accelerationStructure = device.accelerationStructure(asBuilder);

    auto outputTexture = device.texture(engine::TextureInfo{ .dimensions = dimensions });
    engine::ShaderResources shaderResources;
    shaderResources
        .expose(accelerationStructure, engine::ShaderStageFlags::Raygen)
        .expose(outputTexture, engine::ShaderStageFlags::Raygen);

    engine::Shader shader("shaders/tutorial/01_hello_triangle.spv");
    engine::ShaderStages shaderStages;
    shaderStages
        .addRaygen(engine::ShaderStage(shader, "raygenMain"))
        .addGroup(engine::TriangleHitGroup(engine::ShaderStage(shader, "closestHitMain")))
        .addMiss(engine::ShaderStage(shader, "missMain"));

    engine::Renderer renderer(device, shaderResources, shaderStages);

    while (!window.shouldClose()) {
        window.pollEvents();
        renderer.render(device, outputTexture);
    }

    device.wait();
}
