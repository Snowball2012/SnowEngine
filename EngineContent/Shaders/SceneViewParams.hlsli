#pragma once

// @todo - could probably be generated from c++ code?

// must be in sync with GPUSceneViewParams
struct SceneViewParams
{
    float4x4 view_mat;
    float4x4 proj_mat;
    float4x4 view_proj_inv_mat;
    uint2 viewport_size_px;
};

struct TLASItemParams
{
    float3x4 object_to_world_mat;
    uint geom_buf_index;
};

struct MeshVertex
{
    float3 position;
    float3 normal;
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
[[vk::binding( 0, 2 )]] StructuredBuffer<uint16_t> geom_indices[MAX_SCENE_GEOMS];
[[vk::binding( 1, 2 )]] StructuredBuffer<MeshVertex> geom_vertices[MAX_SCENE_GEOMS];
