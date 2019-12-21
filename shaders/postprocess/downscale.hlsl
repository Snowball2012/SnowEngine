#include "../lib/math_utils.hlsli"

Texture2D src : register(t0);

SamplerState linear_clamp_sampler : register(s0);

float4 main(float4 coord : SV_POSITION, float4 ndc : NDCCOORD) : SV_TARGET
{
    // log average

    float2 tc = tc_from_ndc( ndc.xy );
    float2 pixel_size_tc;
    src.GetDimensions(pixel_size_tc.x, pixel_size_tc.y);
    pixel_size_tc = rcp( pixel_size_tc ) * 0.5f;



    float4 s1 = src.Sample( linear_clamp_sampler, tc + pixel_size_tc*float2(-1,-1) );
    float4 s2 = src.Sample( linear_clamp_sampler, tc + pixel_size_tc*float2( 1,-1) );
    float4 s3 = src.Sample( linear_clamp_sampler, tc + pixel_size_tc*float2(-1, 1) );
    float4 s4 = src.Sample( linear_clamp_sampler, tc + pixel_size_tc*float2( 1, 1) );


    //int3 coord2d = int3( coord.x, coord.y, 0 );
    //coord2d *= 2;
    //
    //float4 s1 = src.Load( coord2d );
    //float4 s2 = src.Load( coord2d + int3( 1, 0, 0 ) );
    //float4 s3 = src.Load( coord2d + int3( 0, 1, 0 ) );
    //float4 s4 = src.Load( coord2d + int3( 1, 1, 0 ) );
    //
    const float bias = 1.e-7;
    return exp( ( log( s1 + bias ) + log( s2 + bias ) + log( s3 + bias ) + log( s4 + bias ) ) * 0.25f );
}
