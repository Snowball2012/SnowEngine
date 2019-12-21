#include "lib/colorspaces.hlsli"
#include "lib/math_utils.hlsli"

cbuffer TonemapSettigns : register(b0)
{
    float max_luminance;
    float min_luminance;
    bool blend_luminance;
}

SamplerState linear_clamp_sampler : register(s0);
SamplerState point_clamp_sampler : register(s1);

Texture2D frame : register(t0);

float4 main(float4 coord : SV_POSITION, float4 ndc : NDCCOORD) : SV_TARGET
{
    float2 uv = ((ndc.xy + (float2) 1) / 2);
    uv.y = 1 - uv.y;
    int2 coord2d = int2(coord.x, coord.y);
    float4 cur_radiance = frame.Load(int3(coord2d, 0));
    uint MipLevel; float Width; float Height; float NumberOfLevels;
    frame.GetDimensions( 0, Width, Height, NumberOfLevels );

    float avg_mip = NumberOfLevels - 2;
    float4 avg_radiance[9];
    avg_radiance[0] = frame.SampleLevel( linear_clamp_sampler, uv.xy, avg_mip, int2(0,0) );
    avg_radiance[1] = frame.SampleLevel( linear_clamp_sampler, uv.xy, avg_mip, int2(1,0) );
    avg_radiance[2] = frame.SampleLevel( linear_clamp_sampler, uv.xy, avg_mip, int2(0,1) );
    avg_radiance[3] = frame.SampleLevel( linear_clamp_sampler, uv.xy, avg_mip, int2(1,1) );
    avg_radiance[4] = frame.SampleLevel( linear_clamp_sampler, uv.xy, avg_mip, int2(-1,0) );
    avg_radiance[5] = frame.SampleLevel( linear_clamp_sampler, uv.xy, avg_mip, int2(0,-1) );
    avg_radiance[6] = frame.SampleLevel( linear_clamp_sampler, uv.xy, avg_mip, int2(-1,-1) );
    avg_radiance[7] = frame.SampleLevel( linear_clamp_sampler, uv.xy, avg_mip, int2(-1,1) );
    avg_radiance[8] = frame.SampleLevel( linear_clamp_sampler, uv.xy, avg_mip, int2(1,-1) );
    [unroll]
    float4 avg_radiance_gauss = 0;
    for ( int i = 0; i < 9; ++i )
    {
        avg_radiance_gauss += log( avg_radiance[i] + 1.0e-7f ) * GAUSS_KERNEL_3X3_SIGMA1[i];
    }
    avg_radiance_gauss = exp( avg_radiance_gauss );


    //float4 avg_radiance = frame.SampleLevel( linear_clamp_sampler, uv.xy, NumberOfLevels - 3 );
    
    float cur_luminance = photopic_luminance( cur_radiance.rgb );
    float avg_luminance = photopic_luminance( avg_radiance_gauss.rgb );

    float max_local_luminance = avg_luminance * 8;
    float linear_brightness = saturate( (cur_luminance - min_luminance) / (max_local_luminance - min_luminance) );

    return float4( radiance2gamma_rgb( cur_radiance.rgb, linear_brightness ), cur_radiance.a );
}