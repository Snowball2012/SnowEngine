#ifndef SHADOWS_HLSLI
#define SHADOWS_HLSLI

#include "math_utils.hlsli"

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

    [unroll]
    for ( int i = 0; i < 9; ++i )
        percent_lit += shadow_map.SampleCmpLevelZero( shadow_map_sampler, shadow_pos_h.xy + OFFSETS_3X3[i] * dx, shadow_pos_h.z ).r
                       * GAUSS_KERNEL_3X3_SIGMA1[i];

    return percent_lit;
}

// TODO: generalize somehow?
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
    
    [unroll]
    for ( int i = 0; i < 9; ++i )
        percent_lit += shadow_map.SampleCmpLevelZero( shadow_map_sampler,
                                                      shadow_pos_h.xyz + float3( OFFSETS_3X3[i] * dx, 0 ),
                                                      fragment_z ).r
                       * GAUSS_KERNEL_3X3_SIGMA1[i];

    return ( sin(( percent_lit - 0.5f ) * PI ) + 1.0f ) / 2.0f; // hard shadows effect without pixelization
}

#endif // SHADOWS_HLSLI