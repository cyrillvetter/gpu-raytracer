RaytracingAccelerationStructure accelerationStructure;

struct Camera {
    float meterPerPixel;
    float focalLength;
    float4 origin;
    float4x4 rotation;
};
ConstantBuffer<Camera, ScalarDataLayout> camera;

struct Sphere {
    float3 center;
    float radius;
}
StructuredBuffer<Sphere> spheres;

RWTexture2D<float4> outImage;

struct HitPayload {
    float3 color;
}

[shader("raygeneration")]
void raygenMain() {
    const int2 launchId = DispatchRaysIndex().xy;
    const int2 launchSize = DispatchRaysDimensions().xy;

    const float2 coords = (float2(launchSize) * 0.5 - float2(launchId + 0.5)) * camera.meterPerPixel;

    RayDesc ray;
    ray.Origin = camera.origin.xyz;
    ray.Direction = normalize(mul(camera.rotation, float4(coords, camera.focalLength, 0.0)).xyz);
    ray.TMin = 0.0;
    ray.TMax = 1e4;

    HitPayload payload;
    payload.color = float3(0.0);

    TraceRay(accelerationStructure, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    outImage[launchId] = float4(payload.color, 1.0);
}

[shader("closesthit")]
void closestHitMain(inout HitPayload payload, in BuiltInTriangleIntersectionAttributes attr) {
    payload.color = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
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
    payload.color = float3(1.0, 0.05, 0.05);
}

[shader("miss")]
void missMain(inout HitPayload payload) {
    payload.color = float3(0.75, 0.8, 1.0);
}
