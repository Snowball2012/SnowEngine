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

// bindings description:
// bind point #0 - pass specific params
// bind point #1 - scene global params
// per workload params are specified in inline push constants

[[vk::binding( 0, 1 )]] RaytracingAccelerationStructure scene_tlas;
[[vk::binding( 1, 1 )]] ConstantBuffer<SceneViewParams> view_data;