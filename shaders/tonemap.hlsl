#include "lib/colorspaces.hlsli"
#include "lib/math_utils.hlsli"

cbuffer TonemapSettings : register( b0 )
{
    float whitepoint_luminance;
    float enable_reinhard;
}

SamplerState linear_clamp_sampler : register( s0 );

Texture2D frame : register( t0 );
Texture2D avg_radiance_frame : register( t1 );

RWTexture2D<float4> output : register( u0 );

static const int GROUP_SIZE_X = 8;
static const int GROUP_SIZE_Y = 8;

float calc_scene_key( float avg_luminance )
{
    return 1.03f - 2.0f / ( 2.0f + log10( avg_luminance + 1.0f ) );
}

float3 calc_exposure( float avg_luminance )
{
    return calc_scene_key( avg_luminance ) * rcp( max( float3( 1, 1, 1 ) * 1.e-6f, calc_white_point( avg_luminance ) ) );
}

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
    
    float4 cur_radiance = frame.Load( int3( thread.xy, 0 ) );
    
    float3 white_point = calc_white_point( whitepoint_luminance );

	float3 tonemapped = float3( 0, 0, 0 );

	if ( enable_reinhard )
	{
        const bool use_autoexposure = true;

        if ( use_autoexposure )
        {
            // standard "scene key" formula was derived in the assumption it gets fed to Reinhard tonemapper
            // (https://resources.mpi-inf.mpg.de/hdr/peffects/krawczyk05sccg.pdf)
            // It should use real luminance values to make sense (the curve was fitted using real-world luminance values from photography)
            float4 avg_radiance = avg_radiance_frame.SampleLevel( linear_clamp_sampler, float2( 0.5f, 0.5f ), 0 );
            float avg_luminance = photopic_luminance( avg_radiance );
            float exposure = calc_exposure( avg_luminance );

            cur_radiance *= exposure;

            tonemapped = reinhard( cur_radiance );
        }
        else
        {
            tonemapped = reinhard_extended_luminance( cur_radiance, white_point );
        }
    }
    else
    {
        // simple linear tonemapping, no curves
        tonemapped = cur_radiance.rgb / white_point;
    }

    output[thread.xy] = float4( srgb_OETF( tonemapped ), cur_radiance.a );
}
