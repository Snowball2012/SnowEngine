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
    float t;
};

float3 ReconstructBarycentrics( float2 payload_barycentrics )
{
    return float3( payload_barycentrics, saturate( 1.0f - payload_barycentrics.x - payload_barycentrics.y ) );
}

Payload PayloadInit( float max_t )
{
    Payload ret;
    ret.barycentrics = float2( 0, 0 );
    ret.instance_index = INVALID_INSTANCE_INDEX;
    ret.primitive_index = INVALID_INSTANCE_INDEX;
    ret.t = max_t;
    
    return ret;
}

float3 GetHitNormal( Payload payload, out bool hit_valid )
{
    hit_valid = false;
    uint geom_index = -1;
    float3x4 obj_to_world;
    if ( payload.instance_index != INVALID_INSTANCE_INDEX )
    {
        TLASItemParams item_params = tlas_items[ payload.instance_index ];
        
        geom_index = item_params.geom_buf_index;
        obj_to_world = item_params.object_to_world_mat;
    }
    
    float3 out_normal = _float3( 0 );
    if ( geom_index != INVALID_GEOM_INDEX )
    {
        hit_valid = true;
        uint base_index = payload.primitive_index * 3;
        uint16_t i0 = geom_indices[geom_index][base_index + 0];
        uint16_t i1 = geom_indices[geom_index][base_index + 1];
        uint16_t i2 = geom_indices[geom_index][base_index + 2];
        
        MeshVertex v0 = geom_vertices[geom_index][i0];
        MeshVertex v1 = geom_vertices[geom_index][i1];
        MeshVertex v2 = geom_vertices[geom_index][i2];
        
        float3 triangle_normal_ls = normalize( cross( v1.position - v0.position, v2.position - v0.position ) );
        
        float3 triangle_normal_ws = normalize( mul( float3x3( obj_to_world[0].xyz, obj_to_world[1].xyz, obj_to_world[2].xyz ), triangle_normal_ls ) );
        
        out_normal = triangle_normal_ws;
    }
    
    return out_normal;
}

float3 SampleSphereUniform( float2 e )
{
	float phi = 2 * M_PI * e.x;
	float cos_theta = 1 - 2 * e.y;
	float sin_theta = sqrt( 1 - sqr( cos_theta ) );

    return float3( sin_theta * cos( phi ), sin_theta * sin( phi ), cos_theta );
}

float3 SampleHemisphereCosineWeighted( float2 e, float3 halfplane_normal )
{
    float3 h = SampleSphereUniform( e ).xyz;
	return normalize( halfplane_normal * 1.001f + h ); // slightly biased, but does not hit NaN    
}

// https://github.com/skeeto/hash-prospector
uint HashUint32( uint x )
{
	x ^= x >> 16;
	x *= 0xa812d533;
	x ^= x >> 15;
	x *= 0xb278e4ad;
	x ^= x >> 17;
	return x;
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

    Payload payload = PayloadInit( max_t );

    TraceRay( scene_tlas,
        ( RAY_FLAG_NONE ),
        0xFF, 0, 1, 0, ray, payload );
    
    bool is_hit_valid = false;    
    float3 hit_normal_ws = GetHitNormal( payload, is_hit_valid );
    if ( dot( hit_normal_ws, ray_direction ) > 0 )
    {
        hit_normal_ws = -hit_normal_ws;
    }
    float3 hit_position = ray_origin + ray_direction * payload.t + hit_normal_ws * 1.0e-4f;
    
    float3 output_color = _float3( 0 );
    if ( !is_hit_valid )
    {
        output[pixel_id] = float4( output_color, 1 );
        return;
    }
    
    const int n_samples = 64;
    float ao_accum = 0;
    for ( int sample_i = 0; sample_i < n_samples; ++sample_i )
    {
        uint2 e = uint2( sample_i % 8, sample_i / 8 );
        e += pixel_id * uint2( 37, 71 );
        e.x = HashUint32( e.x );
        e.y = HashUint32( e.y );
        float2 ef = float2( e % uint2( 1024, 1024 ) ) / _float2( 1023.0f );
        
        RayDesc ray = { hit_position, 0, SampleHemisphereCosineWeighted( ef, hit_normal_ws ), max_t };
        Payload payload = PayloadInit( max_t );
        
        TraceRay( scene_tlas,
            ( RAY_FLAG_NONE ),
            0xFF, 0, 1, 0, ray, payload );
            
        if ( payload.instance_index == INVALID_INSTANCE_INDEX )
        {
            ao_accum += 1.0f;
        }
    }
    
    float ao = ao_accum / float( n_samples );
    output_color = _float3( ao );
    
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
    payload.t = RayTCurrent();
}