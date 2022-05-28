
cbuffer BlendSettings : register( b0 )
{
    float blend_val;
    float jitter_x;
    float jitter_y;
    float color_window_size;
}

SamplerState point_wrap_sampler : register( s0 );

Texture2D prev_frame : register( t0 );
Texture2D cur_frame : register( t1 );
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
    
    float2 tex_size;
    prev_frame.GetDimensions( tex_size.x, tex_size.y );
    float2 texcoord = float2( float(thread.x) + 0.5f + jitter_x, float(thread.y) + 0.5f + jitter_y ) / tex_size;

    float4 cur_color = cur_frame.SampleLevel( point_wrap_sampler, texcoord, 0 );
    float4 hist_color = prev_frame.Load( int3( thread.x, thread.y, 0 ) );

    // 3x3 neighbourhood clamping 
    const float2 offsets[8] =
    {
        float2( -1, -1 ), float2( 0, -1 ), float2( 1, -1 ),
        float2( -1, 0 ), float2( 1, 0 ),
        float2( -1, 1 ), float2( 0, 1 ), float2( 1, 1 ),
    };

    float4 min_color = cur_color;
    float4 max_color = cur_color;


    [unroll]
    for ( int i = 0; i < 8; ++i )
    {
        float4 color = cur_frame.SampleLevel( point_wrap_sampler, texcoord + offsets[i] / tex_size, 0 );
        min_color = min( color, min_color );
        max_color = max( color, max_color );
    }

    float3 color_window = ( max_color - min_color ).xyz * color_window_size;
    max_color.xyz += color_window;
    min_color.xyz -= color_window;

    hist_color = clamp( hist_color, min_color, max_color );

    output[thread.xy] = lerp( float4( cur_color.xyz, 1 ), float4( hist_color.xyz, 1 ), blend_val );
}
