#pragma once

#include "Utils.hlsli"

// @todo - could probably be generated from c++ code?

// must be in sync with GPUSceneViewParams
struct SceneViewParams
{
    float4x4 view_mat;
    float4x4 proj_mat;
    float4x4 view_proj_inv_mat;
    uint2 viewport_size_px;
    int2 cursor_position_px;
    uint random_uint;
    uint use_accumulation;
    uint2 _pad;
};

struct TLASItemParams
{
    row_major float3x4 object_to_world_mat;
    uint geom_buf_index;
    uint material_index;
    int picking_id;
};

struct MeshVertex
{
    float3 position;
    float3 normal;
};

struct Material
{
    float3 albedo;
    float3 f0;
    float roughness;
};

struct DebugLine
{
    float3 start_ws;
    float3 end_ws;
    float3 color_start;
    float3 color_end;
};

static const uint MAX_DEBUG_LINES = 1024;
struct DebugLineIndirectArgs
{
    uint vertex_per_instance;
    uint num_instances;
    uint start_vertex_location;
    uint start_instance_location;
};

struct ViewFrameReadbackData
{
    int picking_id_under_cursor;
};

// bindings description:
// bind point #0 - pass specific params
// bind point #1 - scene global params
// bind point #2 - global data (meshes, textures)
// per workload params are specified in inline push constants

static const int MAX_SCENE_GEOMS = 256;

[[vk::binding( 0, 1 )]] RaytracingAccelerationStructure scene_tlas;
[[vk::binding( 1, 1 )]] ConstantBuffer<SceneViewParams> view_data;
[[vk::binding( 2, 1 )]] StructuredBuffer<TLASItemParams> tlas_items;
[[vk::binding( 3, 1 )]] RWStructuredBuffer<DebugLine> debug_lines;
[[vk::binding( 4, 1 )]] RWStructuredBuffer<DebugLineIndirectArgs> debug_lines_indirect_args;
[[vk::binding( 5, 1 )]] RWStructuredBuffer<ViewFrameReadbackData> view_frame_readback_data;
[[vk::binding( 6, 1 )]] Texture2D<float4> env_cubemap;
[[vk::binding( 7, 1 )]] SamplerState env_cubemap_sampler;

[[vk::binding( 0, 2 )]] StructuredBuffer<uint16_t> geom_indices[MAX_SCENE_GEOMS];
[[vk::binding( 1, 2 )]] StructuredBuffer<MeshVertex> geom_vertices[MAX_SCENE_GEOMS];

// @todo - untyped data blob? with typed loads for different types of materials
[[vk::binding( 2, 2 )]] StructuredBuffer<Material> materials;

DebugLine MakeDebugVector( float3 start_ws, float3 dir_ws, float3 color_start, float3 color_end )
{
    DebugLine new_line;
    new_line.start_ws = start_ws;
    new_line.end_ws = start_ws + dir_ws;
    new_line.color_start = color_start;
    new_line.color_end = color_end;
    
    return new_line;
}

void AddDebugLine( DebugLine new_line )
{
    uint line_dst_idx;
    InterlockedAdd( debug_lines_indirect_args[0].num_instances, 1, line_dst_idx );
    if ( line_dst_idx < MAX_DEBUG_LINES )
    {
        debug_lines[line_dst_idx] = new_line;
    }
}

// returns 3 floats in [0, 1] range
float3 GetUnitRandomUniform( uint seed, uint2 pixel_id )
{
   // uint seed_hash = HashUint32( seed );
    
    uint pixel_id_hash = HashUint32( HashUint32( ( pixel_id.x & 0xffff ) | ( ( pixel_id.y & 0xffff ) << 16 ) ) + seed + view_data.random_uint );
    
    uint random_uint = pixel_id_hash;// ^ view_data.random_uint;
    
    return saturate( float3( random_uint & 1023, ( random_uint >> 10 ) & 1023, ( random_uint >> 20 ) & 1023 ) / _float3( float( 1023 ) ) );
}

// returns 2 floats in [0, 1] range
float2 GetUnitRandomUniform( uint2 pixel_id )
{    
    uint pixel_id_hash = HashUint32( ( pixel_id.x & 0xffff ) | ( ( pixel_id.y & 0xffff ) << 16 ) );
    
    uint random_uint = pixel_id_hash ^ HashUint32( view_data.random_uint );
    
    return saturate( float2( random_uint & 0xffff, ( random_uint >> 16 ) & 0xffff ) / _float2( float( 0xffff ) ) );
}
