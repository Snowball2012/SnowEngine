
[[vk::binding( 0 )]]
RaytracingAccelerationStructure scene_tlas;

[[vk::binding( 1 )]]
RWTexture2D<float4> output;

float3 PixelPositionToWorld( uint2 pixel_position, float ndc_depth, uint2 viewport_size, float4x4 view_proj_inverse )
{
    return float3( 0, 0, 0 );
}

struct Payload
{
    float hit_distance;
};

[shader( "raygeneration" )]
void VisibilityRGS()
{
    uint2 pixel_id = DispatchRaysIndex().xy;
    float4x4 view_proj_inverse;
    float3 ray_origin = PixelPositionToWorld( pixel_id, 0, uint2( 0, 0 ), view_proj_inverse );

    float3 ray_destination = PixelPositionToWorld( pixel_id, 1, uint2( 0, 0 ), view_proj_inverse );

    float3 ray_direction = ray_destination - ray_origin;

    float max_t = length( ray_direction );
    ray_direction = normalize( ray_direction );

    RayDesc ray = { ray_origin, 0, ray_direction, max_t };

    Payload payload = { 0 };

    TraceRay( scene_tlas,
        ( RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH ),
        0xFF, 0, 1, 0, ray, payload );

    output[pixel_id] = payload.hit_distance;
}