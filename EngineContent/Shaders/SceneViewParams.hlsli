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
};

struct TLASItemParams
{
    row_major float3x4 object_to_world_mat;
    uint geom_buf_index;
};

struct MeshVertex
{
    float3 position;
    float3 normal;
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

[[vk::binding( 0, 2 )]] StructuredBuffer<uint16_t> geom_indices[MAX_SCENE_GEOMS];
[[vk::binding( 1, 2 )]] StructuredBuffer<MeshVertex> geom_vertices[MAX_SCENE_GEOMS];

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

// returns 2 floats in [0, 1] range
float2 GetUnitRandomUniform( uint seed, uint2 pixel_id )
{
    uint seed_hash = HashUint32( seed );
    
    uint pixel_id_hash = HashUint32( ( pixel_id.x & 0xffff ) | ( ( pixel_id.y & 0xffff ) << 16 ) );
    
    uint random_uint = pixel_id_hash ^ seed_hash ^ view_data.random_uint;
    
    return float2( random_uint & 0xffff, ( random_uint >> 16 ) & 0xffff ) / _float2( float( 0xffff ) );
}
