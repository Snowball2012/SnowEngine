
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

// all outputs are not normalized
float3 halfvector( float3 to_source, float3 to_camera )
{
	return to_source + to_camera;
}

float lambert( float3 to_source, float3 normal )
{
	return dot( normal, to_source );
}

float3 fresnel_schlick( float3 fresnel_r0, float source_to_normal_angle_cos )
{
	return fresnel_r0 + ( float3( 1.0f, 1.0f, 1.0f ) - fresnel_r0 ) * pow( 1.0f - source_to_normal_angle_cos, 5 );
}

// correct only if lambert term is above 0 and triangle is facing camera
float3 specular_strength( float3 fresnel_r0, float3 normal, float3 halfvector, float roughness )
{
	float shininess = ( 1.0f - roughness ) * 256.0f;
	float dot_nh = dot( normal, halfvector );
	float3 spec_power = fresnel_schlick( fresnel_r0, dot_nh ) * ( shininess + 8.0f ) / 8.0f * pow( dot_nh, shininess );
	spec_power = spec_power / ( spec_power + 1.0f );
	return  spec_power;
}