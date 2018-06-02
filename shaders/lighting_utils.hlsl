
#define MAX_LIGHTS 16

struct Light
{
	float4x4 shadow_map_mat;
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
	float4x4 model_inv_transpose_mat;
}

cbuffer cbPerMaterial : register( b1 )
{
	float4x4 transform;
	float3 diffuse_fresnel;
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

SamplerState point_wrap_sampler : register( s0 );
SamplerState point_clamp_sampler : register( s1 );
SamplerState linear_wrap_sampler : register( s2 );
SamplerState linear_clamp_sampler : register( s3 );
SamplerState anisotropic_wrap_sampler : register( s4 );
SamplerState anisotropic_clamp_sampler : register( s5 );
SamplerComparisonState shadow_map_sampler : register( s6 );

Texture2D base_color_map : register( t0 );
Texture2D normal_map : register( t1 );
Texture2D specular_map : register( t2 );
Texture2D shadow_map : register( t3 );

float sqr( float a )
{
	return a * a;
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

float diffuse_disney( float roughness, float source_to_normal_cos, float normal_to_eye_cos, float source_to_half_cos )
{
	// disney semi-pbr diffuse model
	float f = sqr( source_to_half_cos ) * roughness * 2.0f - 0.5f;

	return ( 1.0f + f * pow( 1 - source_to_normal_cos, 5 ) )
		* ( 1.0f + f * pow( 1 - normal_to_eye_cos, 5 ) ) / 3.14f;
}

// correct only if lambert term is above 0 and triangle is facing camera
float3 specular_strength( float3 fresnel_r0, float cos_nh, float cos_lh, float3 halfvector, float roughness )
{
	// ggx ndf, shlick approximation for fresnel
	// no self shadowing. todo: add microfacet selfshadowing
	// alpha remapped to [0.5, 1]

	roughness = sqr( lerp( 0.5, 1, roughness ) );
	float alpha2 = sqr( roughness );
	float ndf = alpha2 / ( 3.14 * sqr( ( alpha2 - 1 ) * sqr( cos_nh ) + 1 ) );
	return fresnel_schlick( fresnel_r0, cos_lh ) * ndf;
}

float shadow_factor( float3 pos_w, float4x4 shadow_map_mat )
{
	// 3x3 PCF
	float4 shadow_pos_h = mul( float4( pos_w, 1.0f ), shadow_map_mat );
	shadow_pos_h.xyz /= shadow_pos_h.w;

	shadow_pos_h.x += 1.0f;
	shadow_pos_h.y += 1.0f;
	shadow_pos_h.y = 2.0f - shadow_pos_h.y;

	shadow_pos_h.xy /= 2.0f;

	uint width, height, nmips;
	shadow_map.GetDimensions( 0, width, height, nmips );

	float dx = 1.0f / width;
	float percent_lit = 0.0f;

	const float2 offsets[9] =
	{
		float2( -dx, -dx ), float2( 0.0f, -dx ), float2( dx, -dx ),
		float2( -dx, 0.0f ), float2( 0.0f, 0.0f ), float2( dx, 0.0f ),
		float2( -dx, dx ), float2( 0.0f, dx ), float2( dx, dx )
	};

	//sigma = 1.0f;
	const float gauss_kernel[9] =
	{
		0.077847f,	0.123317f,	0.077847f,
		0.123317f,	0.195346f,	0.123317f,
		0.077847f,	0.123317f,	0.077847f,
	};

	[unroll]
	for ( int i = 0; i < 9; ++i )
		percent_lit += shadow_map.SampleCmpLevelZero( shadow_map_sampler, shadow_pos_h.xy + offsets[i], shadow_pos_h.z ).r * gauss_kernel[i];

	return percent_lit;
}

float4 dbg_show_normal( float2 uv )
{
	return float4( normal_map.Sample( linear_wrap_sampler, uv ).xy, 1.0f, 1.0f );
}
