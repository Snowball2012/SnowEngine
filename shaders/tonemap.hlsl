#include "lib/colorspaces.hlsli"
#include "lib/math_utils.hlsli"

cbuffer TonemapSettigns : register( b0 )
{
    float2 inv_frame_size; // 1 / texture size in pixels
    float frame_mip_levels;
    float bloom_strength;
    float bloom_mip;
}

SamplerState linear_clamp_sampler : register( s0 );

Texture2D frame : register( t0 );

RWTexture2D<float4> output : register( u0 );

static const int GROUP_SIZE_X = 8;
static const int GROUP_SIZE_Y = 8;

[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void main( uint3 _thread : SV_DispatchThreadID, uint3 thread_in_group : SV_GroupThreadID )
{
    uint2 group_offset_in_threads = _thread.xy - thread_in_group.xy;
    uint index = GROUP_SIZE_X * thread_in_group.y + thread_in_group.x;
    uint2 thread;
    
    // demortonize
    {
        uint x = ( ( index & 0xaa ) << 7 ) | ( index & 0x55 );
        x = ( x | ( x >> 1 ) ) & 0x3333;
        x = ( x | ( x >> 2 ) ) & 0x0f0f;
        x = ( x | ( x >> 4 ) ) & 0x00ff;
        thread.x = x & 0xf;
        thread.y = ( x & 0xf0 ) >> 4;
    }
    
    thread.xy += group_offset_in_threads;
    
    float4 cur_radiance = frame.Load(int3(thread.xy, 0));
    
    float2 uv = ( float2( thread.xy ) + float2( 0.5f, 0.5f ) ) * inv_frame_size; // for bilinear filtering

    float avg_mip = frame_mip_levels - 4;
    float4 avg_radiance[2];
    avg_radiance[0] = frame.SampleLevel( linear_clamp_sampler, uv.xy, avg_mip, int2(-1,-1) );
    avg_radiance[1] = frame.SampleLevel( linear_clamp_sampler, uv.xy, avg_mip, int2(0,-1) );
    
    float log_bias = 1;
    
    float avg_radiance_gauss = 0;
    [unroll]
    for ( int i = 0; i < 2; ++i )
    {
        avg_radiance_gauss += log( photopic_luminance(avg_radiance[i].rgb) + log_bias ) * GAUSS_KERNEL_3X3_SIGMA1[i];
    }
    
    avg_radiance[0] = frame.SampleLevel( linear_clamp_sampler, uv.xy, avg_mip, int2(1,-1) );
    avg_radiance[1] = frame.SampleLevel( linear_clamp_sampler, uv.xy, avg_mip, int2(-1,0) );
    [unroll]
    for ( int i = 0; i < 2; ++i )
    {
        avg_radiance_gauss += log( photopic_luminance(avg_radiance[i].rgb) + log_bias ) * GAUSS_KERNEL_3X3_SIGMA1[2 + i];
    }
    
    avg_radiance[0] = frame.SampleLevel( linear_clamp_sampler, uv.xy, avg_mip, int2(0,0) );
    avg_radiance[1] = frame.SampleLevel( linear_clamp_sampler, uv.xy, avg_mip, int2(1,0) );
    [unroll]
    for ( int i = 0; i < 2; ++i )
    {
        avg_radiance_gauss += log( photopic_luminance(avg_radiance[i].rgb) + log_bias ) * GAUSS_KERNEL_3X3_SIGMA1[4 + i];
    }
    
    
    avg_radiance[0] = frame.SampleLevel( linear_clamp_sampler, uv.xy, avg_mip, int2(-1,1) );
    avg_radiance[1] = frame.SampleLevel( linear_clamp_sampler, uv.xy, avg_mip, int2(0,1) );
    [unroll]
    for ( int i = 0; i < 2; ++i )
    {
        avg_radiance_gauss += log( photopic_luminance(avg_radiance[i].rgb) + log_bias ) * GAUSS_KERNEL_3X3_SIGMA1[6 + i];
    }
    
    
    avg_radiance[0] = frame.SampleLevel( linear_clamp_sampler, uv.xy, avg_mip, int2(1,1) );
    [unroll]
    for ( int i = 0; i < 1; ++i )
    {
        avg_radiance_gauss += log( photopic_luminance(avg_radiance[i].rgb) + log_bias ) * GAUSS_KERNEL_3X3_SIGMA1[8 + i];
    }
    
    avg_radiance_gauss = exp( avg_radiance_gauss ) - log_bias;
    
    float avg_luminance = avg_radiance_gauss; //photopic_luminance( avg_radiance_gauss.rgb );

    const float display_max_luminance = 350.0f; // nits

    float max_local_luminance = max( avg_luminance * 8.3f, display_max_luminance ); // +9.2 dB todo: function of absolute luminance ? (at least clamp max_luminance at a monitor level)
    float min_local_luminance = avg_luminance / 31.6f; // -15 dB
    
    
    float3 bloom_radiance = frame.SampleLevel( linear_clamp_sampler, uv.xy, bloom_mip ).rgb;
    float apply_bloom = (max(( photopic_luminance( bloom_radiance ) - max_local_luminance / 2 ), 0) / max_local_luminance) * bloom_strength;
    
    cur_radiance.rgb = cur_radiance.rgb + apply_bloom * bloom_radiance;
    
    float cur_luminance = photopic_luminance( cur_radiance.rgb );
    float linear_brightness = saturate( (cur_luminance - min_local_luminance) / (max_local_luminance - min_local_luminance) );

    output[thread.xy] = float4( radiance2gamma_rgb( cur_radiance.rgb, linear_brightness ), cur_radiance.a );
}
