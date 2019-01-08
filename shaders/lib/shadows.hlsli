#ifndef SHADOWS_HLSLI
#define SHADOWS_HLSLI

#include "lighting.hlsli"

int csm_get_frustrum( float depth, int num_split_positions, float split_positions[MAX_CASCADE_SIZE-1] )
{
    int frustrum = 0;
    for ( int i = 0; i < num_split_positions; i++ )
        frustrum += depth > split_positions[i]; // no branching, since num_split_positions should be ~[2..4]
    return frustrum;
}

float shadow_factor( float3 pos_v, float4x4 shadow_map_mat, Texture2D shadow_map, SamplerComparisonState shadow_map_sampler )
{
	// 3x3 PCF
	float4 shadow_pos_h = mul( float4( pos_v, 1.0f ), shadow_map_mat );
	shadow_pos_h.xyz /= shadow_pos_h.w;

	shadow_pos_h.x += 1.0f;
	shadow_pos_h.y += 1.0f;
	shadow_pos_h.y = 2.0f - shadow_pos_h.y;

	shadow_pos_h.xy *= 0.5f;

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

// ToDo: generalize somehow?
float shadow_factor_array( float3 pos_v, float4x4 shadow_map_mat, Texture2DArray shadow_map, int shadow_map_idx, SamplerComparisonState shadow_map_sampler )
{
	// 3x3 PCF
	float4 shadow_pos_h = mul( float4( pos_v, 1.0f ), shadow_map_mat );
	shadow_pos_h.xyz /= shadow_pos_h.w;

    float fragment_z = shadow_pos_h.z;

	shadow_pos_h.x += 1.0f;
	shadow_pos_h.y += 1.0f;
	shadow_pos_h.y = 2.0f - shadow_pos_h.y;

	shadow_pos_h.xy *= 0.5f;
    shadow_pos_h.z = shadow_map_idx;

	uint width, height, nmips, nlevels;
	shadow_map.GetDimensions( 0, width, height, nmips, nlevels );

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
		percent_lit += shadow_map.SampleCmpLevelZero( shadow_map_sampler, shadow_pos_h.xyz + float3( offsets[i], 0 ), fragment_z ).r * gauss_kernel[i];

	return percent_lit;
}

float csm_shadow_factor( float3 pos_v, ParallelLight light, Texture2DArray shadow_cascade, float split_positions[MAX_CASCADE_SIZE-1], SamplerComparisonState shadow_map_sampler )
{
    int frustrum = csm_get_frustrum( pos_v.z, light.csm_num_split_positions, split_positions );
    return shadow_factor_array( pos_v, light.shadow_map_mat[frustrum], shadow_cascade, frustrum, shadow_map_sampler );
}

#endif // SHADOWS_HLSLI