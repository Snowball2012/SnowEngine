#ifndef PASS_CB_HLSLI
#define PASS_CB_HLSLI

#ifdef PER_PASS_CB_BINDING

#include "../lib/lighting.hlsli"

struct PassConstants
{
    float4x4 view_mat;
	float4x4 view_inv_mat;
	float4x4 proj_mat;
	float4x4 proj_inv_mat;
	float4x4 view_proj_mat;
	float4x4 view_proj_inv_mat;

	float3 eye_pos_w;
	float _padding1;

	float2 render_target_size;
	float2 render_target_size_inv;

	float near_z;
	float far_z;
    float fov_y;
    float aspect_ratio;

	float total_time;
	float delta_time;
    float2 _padding2;

	Light lights[MAX_LIGHTS];

	int n_parallel_lights;
	int n_point_lights;
	int n_spotlight_lights;
    int use_linear_depth;
};

cbuffer cbPerPass : register( PER_PASS_CB_BINDING )
{
	PassConstants pass_params;
}

#endif // PER_PASS_CB_BINDING

#endif // PASS_CB_HLSLI