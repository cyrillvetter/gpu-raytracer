#include <engine/engine.hpp>
#include "../util/window.hpp"
#include <glm/glm.hpp>

const int WIDTH = 1500;
const int HEIGHT = 1000;
const std::string NAME = "06 - Reflection";

struct Vertex {
    glm::vec4 position;
};
const std::vector<Vertex> vertices = {
    {{ -3.0, 0.0, -2.0, 0.0 }},
    {{  3.0, 0.0, -2.0, 0.0 }},
    {{  3.0, 0.0,  2.0, 0.0 }},
    {{ -3.0, 0.0,  2.0, 0.0 }}
};
const std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };

struct Sphere {
    glm::vec3 center;
    float radius;
    glm::vec3 color;
    uint32_t hitGroupIndex;

    engine::AABB aabb() const {
        return engine::AABB{ glm::vec3(-radius), glm::vec3(radius) };
    }

    glm::mat4x3 mat() const {
        glm::mat4x3 transform(1.0f);
        transform[3] = center;
        return transform;
    }
};

struct Camera {
    float meterPerPixel;
    float focalLength;
    glm::vec4 origin;
    glm::mat4 rotation;
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

    std::array<Sphere, 5> spheres = {
        Sphere{ {  1.6, 0.45,  1.4 }, 0.45, { 0.20, 0.36, 0.40 }, 1 },
        Sphere{ {  0.6, 0.30,  0.0 }, 0.30, { 1.00, 0.95, 0.69 }, 1 },
        Sphere{ { -1.2, 0.20,  0.6 }, 0.20, { 0.88, 0.62, 0.24 }, 2 },
        Sphere{ { -0.2, 0.25, -0.4 }, 0.25, { 0.62, 0.17, 0.17 }, 1 },
        Sphere{ { -0.9, 0.10, -0.5 }, 0.10, { 0.33, 0.04, 0.06 }, 1 },
    };
    auto sphereBuffer = device.structuredBuffer<Sphere>(spheres.size());
    sphereBuffer.fill(spheres.data());

    engine::AccelerationStructureBuilder asBuilder;
    asBuilder.add(vertexBuffer, indexBuffer, { .hitGroupIndex = 0 });
    for (uint32_t i = 0; i < spheres.size(); ++i)
        asBuilder.add(spheres[i].aabb(), { .instanceId = i, .transform = spheres[i].mat(), .hitGroupIndex = spheres[i].hitGroupIndex });
    auto accelerationStructure = device.accelerationStructure(asBuilder);

    Camera camera{
        .meterPerPixel = 1.0 / static_cast<float>(WIDTH),
        .focalLength = 0.6,
        .origin = glm::vec4(0.0, 0.7, -2.0, 0.0),
        .rotation = glm::mat4(1.0)
    };
    auto cameraBuffer = device.constantBuffer<Camera>();
    cameraBuffer.fill(&camera);

    auto outputTexture = device.texture(engine::TextureInfo{ .dimensions = dimensions });
    auto accumulationTexture = device.texture(engine::TextureInfo{
        .dimensions = dimensions,
        .format = engine::Format::eR32G32B32A32Sfloat
    });
    engine::PushConstant<uint32_t> frameConstant;

    engine::ShaderResources shaderResources;
    shaderResources
        .expose(accelerationStructure, engine::ShaderStageFlags::Raygen)
        .expose(cameraBuffer, engine::ShaderStageFlags::Raygen)
        .expose(sphereBuffer, engine::ShaderStageFlags::Intersection | engine::ShaderStageFlags::ClosestHit)
        .expose(frameConstant, engine::ShaderStageFlags::Raygen | engine::ShaderStageFlags::ClosestHit)
        .expose(accumulationTexture, engine::ShaderStageFlags::Raygen)
        .expose(outputTexture, engine::ShaderStageFlags::Raygen);

    engine::Shader shader("shaders/main.spv");
    engine::ShaderStage intersect(shader, "sphereIntersect");
    engine::ShaderStages shaderStages;
    shaderStages
        .addRaygen(engine::ShaderStage(shader, "raygenMain"))
        .addGroup(engine::TriangleHitGroup(engine::ShaderStage(shader, "closestHitMain")))
        .addGroup(engine::ProceduralHitGroup(intersect, engine::ShaderStage(shader, "sphereClosest")))
        .addGroup(engine::ProceduralHitGroup(intersect, engine::ShaderStage(shader, "reflectiveSphereClosest")))
        .addMiss(engine::ShaderStage(shader, "missMain"));

    engine::Renderer renderer(device, shaderResources, shaderStages);

    uint32_t frameIndex = 1;

    while (!window.shouldClose()) {
        window.pollEvents();
        renderer.pushConstant(frameConstant, &frameIndex);
        renderer.render(device, outputTexture);
        frameIndex++;
    }

    device.wait();
}
