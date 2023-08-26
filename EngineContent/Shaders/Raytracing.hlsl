
#include "SceneViewParams.hlsli"

[[vk::binding( 0, 0 )]] RWTexture2D<float4> output;

float3 PixelPositionToWorld( uint2 pixel_position, float ndc_depth, uint2 viewport_size, float4x4 view_proj_inverse )
{
    float4 ndc = float4( float2( pixel_position ) / float2( viewport_size ) * 2.0f - float2( 1.0f, 1.0f ), ndc_depth, 1.0f );

    float4 world_pos = mul( view_proj_inverse, ndc );

    world_pos /= world_pos.w;

    return world_pos.xyz;
}

struct Payload
{
    float2 hit_distance;
};

[shader( "raygeneration" )]
void VisibilityRGS()
{
    uint2 pixel_id = DispatchRaysIndex().xy;

    float3 ray_origin = PixelPositionToWorld( pixel_id, 0, view_data.viewport_size_px, view_data.view_proj_inv_mat );

    float3 ray_destination = PixelPositionToWorld( pixel_id, 1.0f, view_data.viewport_size_px, view_data.view_proj_inv_mat );

    float3 ray_direction = ray_destination - ray_origin;

    float max_t = length( ray_direction );
    ray_direction = normalize( ray_direction );

    RayDesc ray = { ray_origin, 0, ray_direction, max_t };

    Payload payload = { 0, 0 };

    TraceRay( scene_tlas,
        ( RAY_FLAG_NONE ),
        0xFF, 0, 1, 0, ray, payload );
    
    output[pixel_id] = float4( payload.hit_distance, 0, 1 );
}

[shader( "miss" )]
void VisibilityRMS( inout Payload payload )
{
    payload.hit_distance = float2( 0,0 );
}

[shader( "closesthit" )]
void VisibilityRCS( inout Payload payload, in BuiltInTriangleIntersectionAttributes attr )
{
    payload.hit_distance = attr.barycentrics.xy;
}