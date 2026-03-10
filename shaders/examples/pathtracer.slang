RaytracingAccelerationStructure accelerationStructure;

struct CameraProperties {
    float meterPerPixel;
    float focalLength;
    float4 origin;
    float4x4 rotation;
};
ConstantBuffer<CameraProperties, ScalarDataLayout> camera;

RWTexture2D<float4> outTexture;
RWTexture2D<float4> accumulationTexture;

struct Vertex {
    float3 position;
    float3 normal;
};
StructuredBuffer<Vertex> vertices[];
StructuredBuffer<uint> indices[];
StructuredBuffer<uint> materialIndices;

struct PushConstant {
    uint frame;
    uint inc;
    float rand;
}
[vk::push_constant] const PushConstant pc;

struct Material {
    float3 color;
    float roughness;
};
StructuredBuffer<Material> materials;

struct HitPayload {
    float3 color;
    float2 launchId;
    Optional<RayDesc> ray;
};

static const uint MAX_DEPTH = 8;
static const float GLASS_IOR = 1.52;

// Hash functions from: https://www.shadertoy.com/view/4djSRW

float hash11(float p) {
    p = fract(p * .1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

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

struct HitRecord {
    float3 position;
    float3 normal;
}

HitRecord createHitRecord(const BuiltInTriangleIntersectionAttributes attr) {
    uint instanceId = InstanceID();
    uint primitiveIndex = PrimitiveIndex();

    const var indexBuffer = indices[NonUniformResourceIndex(instanceId)];
    const var vertexBuffer = vertices[NonUniformResourceIndex(instanceId)];

    const uint i0 = indexBuffer[primitiveIndex * 3 + 0];
    const uint i1 = indexBuffer[primitiveIndex * 3 + 1];
    const uint i2 = indexBuffer[primitiveIndex * 3 + 2];

    const Vertex v0 = vertexBuffer[i0];
    const Vertex v1 = vertexBuffer[i1];
    const Vertex v2 = vertexBuffer[i2];

    const float3 bary = float3(1.0f - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    const float3 position = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    const float3 normal = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);

    HitRecord record;
    record.position = position;
    record.normal = normal;
    return record;
}

[shader("raygeneration")]
void raygenMain() {
    int2 launchId = DispatchRaysIndex().xy;
    int2 launchSize = DispatchRaysDimensions().xy;

    HitPayload payload;
    payload.color = float3(0.0);
    payload.launchId = float2(launchId.xy);

    float planeX = (float(launchId.x) + (hash12(launchId.xy + pc.rand) - 0.5) - float(launchSize.x) * 0.5) * camera.meterPerPixel;
    float planeY = (float(launchSize.y) * 0.5 - float(launchId.y) + (hash12(launchId.yx + pc.rand + pc.inc) - 0.5)) * camera.meterPerPixel;

    RayDesc ray;
    ray.Origin = camera.origin.xyz;
    ray.Direction = mul(camera.rotation, float4(planeX, planeY, camera.focalLength, 1.0f)).xyz;
    ray.TMin = 0.0;
    ray.TMax = 1e4;

    payload.ray = ray;
    float3 color = float3(1.0f);

    for (uint j = MAX_DEPTH; j >= 0; --j) {
        if (j == 0) {
            color = float3(0.0f);
            break;
        }

        TraceRay(accelerationStructure, RAY_FLAG_FORCE_NON_OPAQUE, 0xFF, 0, 1, 0, payload.ray.value, payload);
        color *= payload.color;

        if (!payload.ray.hasValue) {
            break;
        }
    }

    if (pc.frame == 1) {
        accumulationTexture[launchId] = float4(color, 1.0);
    } else {
        accumulationTexture[launchId] += float4(color, 1.0);
    }

    outTexture[launchId] = float4(accumulationTexture[launchId].rgb / float(pc.frame), 1.0);
}

[shader("closesthit")]
void diffuseClosestHitMain(inout HitPayload payload, in BuiltInTriangleIntersectionAttributes attr) {
    const HitRecord record = createHitRecord(attr);
    const Material mat = materials[materialIndices[InstanceID()]];
    const float3 scatteredDir = normalize(record.normal + normalize(signedHash32(payload.launchId + pc.rand + record.position.zx + float(pc.inc))));

    RayDesc scatteredRay;
    scatteredRay.Origin = record.position;
    scatteredRay.Direction = scatteredDir;
    scatteredRay.TMin = 0.0001f;
    scatteredRay.TMax = 1e4;

    payload.ray = scatteredRay;
    payload.color = mat.color;
}

[shader("closesthit")]
void specularClosestHitMain(inout HitPayload payload, in BuiltInTriangleIntersectionAttributes attr) {
    const HitRecord record = createHitRecord(attr);
    const Material mat = materials[materialIndices[InstanceID()]];

    RayDesc scatteredRay;
    scatteredRay.Origin = record.position;
    scatteredRay.TMin = 0.0001f;
    scatteredRay.TMax = 1e4;

    if (hash11(pc.rand + payload.launchId.x + record.position.z) > mat.roughness) {
        scatteredRay.Direction = normalize(reflect(WorldRayDirection(), record.normal));
    } else {
        scatteredRay.Direction = normalize(record.normal + normalize(signedHash32(payload.launchId + pc.rand + record.position.zx + float(pc.inc))));
    }

    payload.ray = scatteredRay;
    payload.color = mat.color;
}

[shader("closesthit")]
void reflectiveClosestHitMain(inout HitPayload payload, in BuiltInTriangleIntersectionAttributes attr) {
    const HitRecord record = createHitRecord(attr);
    const Material mat = materials[materialIndices[InstanceID()]];
    const float3 reflectionDir = normalize(reflect(WorldRayDirection(), record.normal));

    RayDesc reflectionRay;
    reflectionRay.Origin = record.position;
    reflectionRay.Direction = reflectionDir;
    reflectionRay.TMin = 0.0001f;
    reflectionRay.TMax = 1e4;

    payload.ray = reflectionRay;
    payload.color = mat.color;
}

// Schlick's approximation.
float reflectance(float cosine, float ior) {
    const float r0 = (1.0 - ior) / pow(1.0 + ior, 2.0);
    return r0 + (1.0 - r0) * pow(1.0 - cosine, 5.0);
}

[shader("closesthit")]
void refractiveClosestHitMain(inout HitPayload payload, in BuiltInTriangleIntersectionAttributes attr) {
    HitRecord record = createHitRecord(attr);
    const Material mat = materials[materialIndices[InstanceID()]];

    float eta = GLASS_IOR;
    if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE) {
        eta = 1.0f / GLASS_IOR;
    } else {
        record.normal *= -1.0;
    }

    const float cos_theta = min(dot(-WorldRayDirection(), record.normal), 1.0);
    const float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    const bool cannot_refract = eta * sin_theta > 1.0;

    RayDesc refractionRay;
    refractionRay.Origin = record.position;
    refractionRay.TMin = 0.0001f;
    refractionRay.TMax = 1e4;

    if (cannot_refract || reflectance(cos_theta, eta) > hash11(payload.launchId.x + payload.launchId.y + record.position.z + pc.rand)) {
        refractionRay.Direction = reflect(WorldRayDirection(), record.normal);
    } else {
        refractionRay.Direction = refract(WorldRayDirection(), record.normal, eta);
    }

    payload.ray = refractionRay;
    payload.color = mat.color;
}

[shader("closesthit")]
void emissiveClosestHitMain(inout HitPayload payload, in BuiltInTriangleIntersectionAttributes attr) {
    const Material mat = materials[materialIndices[InstanceID()]];
    payload.ray = none;
    payload.color = mat.color;
}

[shader("miss")]
void missMain(inout HitPayload payload) {
    const float a = 0.5 * (WorldRayDirection().y + 1.0);
    payload.color = min((1.0 - a) * float3(1.0) + a * float3(0.45, 0.65, 0.9), float3(1.0));
    payload.ray = none;
}
