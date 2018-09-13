#ifndef PASS_CB_HLSLI
#define PASS_CB_HLSLI

#ifdef PER_PASS_CB_BINDING

#include "../lib/lighting.hlsli"

cbuffer cbPerPass : register( PER_PASS_CB_BINDING )
{
	float4x4 view_mat;
	float4x4 view_inv_mat;
	float4x4 proj_mat;
	float4x4 proj_inv_mat;
	float4x4 view_proj_mat;
	float4x4 view_proj_inv_mat;

	float3 eye_pos_w;
	float cb_per_object_pad_1;

	float2 render_target_size;
	float2 render_target_size_inv;

	float near_z;
	float far_z;
    float fov_y;
    float aspect_ratio;

	float total_time;
	float delta_time;
    float2 _padding;

	Light lights[MAX_LIGHTS];

	int n_parallel_lights = 0;
	int n_point_lights = 0;
	int n_spotlight_lights = 0;

    int use_linear_depth;
}

#endif // PER_PASS_CB_BINDING

#endif // PASS_CB_HLSLI