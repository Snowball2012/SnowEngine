#ifndef SHADOWS_HLSLI
#define SHADOWS_HLSLI

int csm_get_split( float depth, float near_z, float far_z, int nsplits )
{

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

#endif // SHADOWS_HLSLI