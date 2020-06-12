#include "lib/colorspaces.hlsli"
#include "lib/math_utils.hlsli"

cbuffer TonemapSettigns : register( b0 )
{
    float2 inv_frame_size; // 1 / texture size in pixels
    float frame_mip_levels;
}

SamplerState linear_clamp_sampler : register( s0 );

Texture2D frame : register( t0 );

RWTexture2D<float4> output : register( u0 );

static const int GROUP_SIZE_X = 8;
static const int GROUP_SIZE_Y = 8;

[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void main( uint3 thread : SV_DispatchThreadID )
{
    float4 cur_radiance = frame.Load(int3(thread.xy, 0));
    
    float2 uv = ( float2( thread.xy ) + float2( 0.5f, 0.5f ) ) * inv_frame_size; // for bilinear filtering

    float avg_mip = frame_mip_levels - 3;
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
    
    float cur_luminance = photopic_luminance( cur_radiance.rgb );
    float avg_luminance = photopic_luminance( avg_radiance_gauss.rgb );

    const float display_max_luminance = 350.0f; // nits

    float max_local_luminance = max( avg_luminance * 8.3f, display_max_luminance ); // +9.2 dB todo: function of absolute luminance ? (at least clamp max_luminance at a monitor level)
    float min_local_luminance = avg_luminance / 31.6f; // -15 dB
    float linear_brightness = saturate( (cur_luminance - min_local_luminance) / (max_local_luminance - min_local_luminance) );

    output[thread.xy] = float4( radiance2gamma_rgb( cur_radiance.rgb, linear_brightness ), cur_radiance.a );
}
