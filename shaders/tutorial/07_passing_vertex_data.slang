RaytracingAccelerationStructure accelerationStructure;

struct Camera {
    float meterPerPixel;
    float focalLength;
    float4 origin;
    float4x4 rotation;
};
ConstantBuffer<Camera, ScalarDataLayout> camera;

struct Vertex {
    float3 position;
    float3 normal;
}
StructuredBuffer<Vertex> vertices[];
StructuredBuffer<uint> indices[];

struct Sphere {
    float3 center;
    float radius;
    float3 color;
    uint hitGroupIndex;
}
StructuredBuffer<Sphere> spheres;

[vk::push_constant] const uint frame;

RWTexture2D<float4> accumulationTexture;
RWTexture2D<float4> outImage;

struct HitPayload {
    float3 color;
    float2 launchId;
    Optional<RayDesc> ray;
}

struct HitRecord {
    float3 position;
    float3 normal;
}

// Hash functions from: https://www.shadertoy.com/view/4djSRW

float hash12(float2 p) {
    float3 p3  = fract(float3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float3 signedHash32(float2 p) {
    float3 p3 = fract(float3(p.xyx) * float3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yxz+33.33);
    return fract((p3.xxy+p3.yzz)*p3.zyx) * 2.0 - 1.0;
}

float3 diffuseScatter(HitPayload payload, HitRecord record) {
    return normalize(record.normal + normalize(signedHash32(payload.launchId + record.position.zx + float(frame))));
}

static const uint MAX_DEPTH = 6;

[shader("raygeneration")]
void raygenMain() {
    const int2 launchId = DispatchRaysIndex().xy;
    const int2 launchSize = DispatchRaysDimensions().xy;

    const float cx = (float(launchId.x) + (hash12(launchId.xy + frame) - 0.5) - float(launchSize.x) * 0.5) * camera.meterPerPixel;
    const float cy = (float(launchSize.y) * 0.5 - float(launchId.y) + (hash12(launchId.yx + frame) - 0.5)) * camera.meterPerPixel;

    RayDesc ray;
    ray.Origin = camera.origin.xyz;
    ray.Direction = normalize(mul(camera.rotation, float4(cx, cy, camera.focalLength, 0.0)).xyz);
    ray.TMin = 0.0;
    ray.TMax = 1e4;

    HitPayload payload;
    payload.color = float3(0.0);
    payload.launchId = float2(launchId);
    payload.ray = ray;

    float3 color = float3(1.0);

    for (uint i = MAX_DEPTH; i >= 0; --i) {
        if (i == 0) {
            color = float3(0.0);
            break;
        }

        TraceRay(accelerationStructure, RAY_FLAG_NONE, 0xFF, 0, 1, 0, payload.ray.value, payload);
        color *= payload.color;

        if (!payload.ray.hasValue) {
            break;
        }
    }

    accumulationTexture[launchId] += float4(color, 1.0);
    outImage[launchId] = float4(accumulationTexture[launchId].rgb / float(frame), 1.0);
}

HitRecord unpackTriangle(float2 bary) {
    const uint instanceId = InstanceID();
    const uint primitiveIndex = PrimitiveIndex();

    const var indexBuffer = indices[NonUniformResourceIndex(instanceId)];
    const var vertexBuffer = vertices[NonUniformResourceIndex(instanceId)];

    const uint i0 = indexBuffer[primitiveIndex * 3 + 0];
    const uint i1 = indexBuffer[primitiveIndex * 3 + 1];
    const uint i2 = indexBuffer[primitiveIndex * 3 + 2];

    const float3 n0 = vertexBuffer[i0].normal;
    const float3 n1 = vertexBuffer[i1].normal;
    const float3 n2 = vertexBuffer[i2].normal;

    const float3 barCords = float3(1.0 - bary.x - bary.y, bary.x, bary.y);
    const float3 position = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    const float3 normal = normalize(n0 * barCords.x + n1 * barCords.y + n2 * barCords.z);

    HitRecord record;
    record.position = position;
    record.normal = normal;
    return record;
}

[shader("closesthit")]
void closestHitMain(inout HitPayload payload, in BuiltInTriangleIntersectionAttributes attr) {
    const HitRecord record = unpackTriangle(attr.barycentrics);

    RayDesc ray;
    ray.Origin = record.position;
    ray.Direction = diffuseScatter(payload, record);
    ray.TMin = 0.001f;
    ray.TMax = 1e4;

    payload.color = float3(0.55, 0.54, 0.47);
    payload.ray = ray;
}

// Ray-sphere intersection
// By Inigo Quilez, from https://iquilezles.org/articles/spherefunctions/
float sphIntersect(const Sphere s, float3 ro, float3 rd) {
    float3 oc = ro - s.center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - s.radius * s.radius;
    float h = b * b - c;
    if (h < 0.0) {
        return -1.0;
    }
    h = sqrt(h);
    return -b - h;
}

[shader("intersection")]
void sphereIntersect() {
    const Sphere sphere = spheres[InstanceID()];
    float hit = sphIntersect(sphere, WorldRayOrigin(), WorldRayDirection());

    if (hit > 0.0) {
        ReportHit(hit, 0, 0);
    }
}

[shader("closesthit")]
void sphereClosest(inout HitPayload payload) {
    const Sphere sphere = spheres[InstanceID()];

    HitRecord record;
    record.position = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    record.normal = normalize(record.position - sphere.center);

    RayDesc ray;
    ray.Origin = record.position;
    ray.Direction = diffuseScatter(payload, record);
    ray.TMin = 0.001f;
    ray.TMax = 1e4;

    payload.color = sphere.color;
    payload.ray = ray;
}

[shader("closesthit")]
void reflectiveSphereClosest(inout HitPayload payload) {
    const Sphere sphere = spheres[InstanceID()];
    const float3 pos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    const float3 normal = normalize(pos - sphere.center);

    RayDesc ray;
    ray.Origin = pos;
    ray.Direction = normalize(reflect(WorldRayDirection(), normal));
    ray.TMin = 0.001f;
    ray.TMax = 1e4;

    payload.color = sphere.color;
    payload.ray = ray;
}

[shader("miss")]
void missMain(inout HitPayload payload) {
    payload.color = float3(0.75, 0.8, 1.0);
    payload.ray = none;
}
