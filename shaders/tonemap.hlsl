#include "lib/colorspaces.hlsli"
#include "lib/math_utils.hlsli"

cbuffer TonemapSettigns : register( b0 )
{
    float whitepoint_luminance;
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
    
    float4 cur_radiance = frame.Load( int3( thread.xy, 0 ) );
    
    float3 white_point = calc_white_point( whitepoint_luminance );

    // simple linear tonemapping, no curves
    float3 normalized_color = cur_radiance.rgb / white_point;

    output[thread.xy] = float4( srgb_OETF( normalized_color ), cur_radiance.a );
}
