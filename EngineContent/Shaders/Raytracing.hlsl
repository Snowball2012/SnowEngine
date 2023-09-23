#include "Utils.hlsli"

#include "SceneViewParams.hlsli"

[[vk::binding( 0, 0 )]] RWTexture2D<float4> output;

float3 PixelPositionToWorld( uint2 pixel_position, float ndc_depth, uint2 viewport_size, float4x4 view_proj_inverse )
{
    float4 ndc = float4( float2( pixel_position ) / float2( viewport_size ) * 2.0f - float2( 1.0f, 1.0f ), ndc_depth, 1.0f );
    ndc.y = -ndc.y;

    float4 world_pos = mul( view_proj_inverse, ndc );

    world_pos /= world_pos.w;

    return world_pos.xyz;
}

static const uint INVALID_PRIMITIVE = -1;
static const uint INVALID_INSTANCE_INDEX = -1;
static const uint INVALID_GEOM_INDEX = -1;

struct Payload
{
    float2 barycentrics;
    uint instance_index;
    uint primitive_index;
};

float3 ReconstructBarycentrics( float2 payload_barycentrics )
{
    return float3( payload_barycentrics, saturate( 1.0f - payload_barycentrics.x - payload_barycentrics.y ) );
}

Payload PayloadInit()
{
    Payload ret;
    ret.barycentrics = float2( 0, 0 );
    ret.instance_index = INVALID_INSTANCE_INDEX;
    ret.primitive_index = INVALID_INSTANCE_INDEX;
    
    return ret;
}

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

    Payload payload = PayloadInit();

    TraceRay( scene_tlas,
        ( RAY_FLAG_NONE ),
        0xFF, 0, 1, 0, ray, payload );
        
    uint geom_index = -1;
    float3x4 obj_to_world;
    if ( payload.instance_index != INVALID_INSTANCE_INDEX )
    {
        TLASItemParams item_params = tlas_items[ payload.instance_index ];
        
        geom_index = item_params.geom_buf_index;
        obj_to_world = item_params.object_to_world_mat;
    }
    
    float3 output_color = _float3( 0 );
    if ( geom_index != INVALID_GEOM_INDEX )
    {
        uint base_index = payload.primitive_index * 3;
        uint16_t i0 = geom_indices[geom_index][base_index + 0];
        uint16_t i1 = geom_indices[geom_index][base_index + 1];
        uint16_t i2 = geom_indices[geom_index][base_index + 2];
        
        MeshVertex v0 = geom_vertices[geom_index][i0];
        MeshVertex v1 = geom_vertices[geom_index][i1];
        MeshVertex v2 = geom_vertices[geom_index][i2];
        
        float3 triangle_normal_ls = normalize( cross( v1.position - v0.position, v2.position - v0.position ) );
        
        float3 triangle_normal_ws = normalize( mul( float3x3( obj_to_world[0].xyz, obj_to_world[1].xyz, obj_to_world[2].xyz ), triangle_normal_ls ) );
        
        output_color = triangle_normal_ws * 0.5f + _float3( 0.5f );
    }
    else
    {
        output_color = float3( 0, 0.5f, 0.5f );
    }
    
    //output[pixel_id] = float4( ColorFromIndex( geom_index ), 1 );
    output[pixel_id] = float4( output_color, 1 );
    //output[pixel_id] = float4( ReconstructBarycentrics( payload.barycentrics ), 1 );
}

[shader( "miss" )]
void VisibilityRMS( inout Payload payload )
{
}

[shader( "closesthit" )]
void VisibilityRCS( inout Payload payload, in BuiltInTriangleIntersectionAttributes attr )
{
    payload.barycentrics = attr.barycentrics.xy;
    payload.instance_index = InstanceIndex();
    payload.primitive_index = PrimitiveIndex();
}