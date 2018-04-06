
#define MAX_LIGHTS 16

struct Light
{
	float3 strength;
	float falloff_start;
	float3 origin;
	float falloff_end;
	float3 dir;
	float spot_power;
};

cbuffer cbPerObject : register( b0 )
{
	float4x4 model_mat;
}

cbuffer cbPerMaterial : register( b1 )
{
	float4x4 transform;
	float4 diffuse_albedo;
	float3 fresnel_r0;
	float roughness;
}

cbuffer cbPerPass : register( b2 )
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
	float total_time;
	float delta_time;

	Light lights[MAX_LIGHTS];
	int n_parallel_lights = 0;
	int n_point_lights = 0;
	int n_spotlight_lights = 0;
}

// TODO: rendering equation