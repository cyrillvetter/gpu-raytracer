RaytracingAccelerationStructure accelerationStructure;
RWTexture2D<float4> outImage;

struct HitPayload {
    float3 color;
}

static const float cameraDepth = -1.0;

[shader("raygeneration")]
void raygenMain() {
    const int2 launchId = DispatchRaysIndex().xy;
    const int2 launchSize = DispatchRaysDimensions().xy;

    float2 coords = float2(launchId) / float2(launchSize) * 2.0 - 1.0;
    coords.x *= float(launchSize.x) / float(launchSize.y);
    const float3 origin = float3(coords, cameraDepth);

    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = float3(0.0, 0.0, 1.0);
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

[shader("miss")]
void missMain(inout HitPayload payload) {
    payload.color = float3(0.75, 0.8, 1.0);
}
