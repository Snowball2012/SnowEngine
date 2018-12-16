// Dimensions for output and depth textures must match

#define PER_PASS_CB_BINDING b0
#include "../bindings/pass_cb.hlsli"

Texture2D input : register( t0 );
Texture2D depth : register( t1 );

RWTexture2D<min16float> output : register( u0 );

SamplerState linear_wrap_sampler : register( s0 );

float LinearDepth( float hyperbolic_z )
{
    return ( ( pass_params.near_z / ( pass_params.near_z - pass_params.far_z ) ) * pass_params.far_z )
           / ( hyperbolic_z - ( pass_params.far_z / ( pass_params.far_z - pass_params.near_z ) ) );
}


[numthreads(64, 4, 1)]
void main( uint3 thread : SV_DispatchThreadID )
{
    // we're fine with a potential underflow, reads from inaccessible mem return 0 anyway
    const int noffsets = 49;
    int2 offsets[noffsets] =
    {
        int2( -3, -3 ), int2( -2, -3 ), int2( -1, -3 ), int2(  0, -3 ), int2(  1, -3 ), int2( 2, -3 ), int2( 3, -3 ), 
        int2( -3, -2 ), int2( -2, -2 ), int2( -1, -2 ), int2(  0, -2 ), int2(  1, -2 ), int2( 2, -2 ), int2( 3, -2 ), 
        int2( -3, -1 ), int2( -2, -1 ), int2( -1, -1 ), int2(  0, -1 ), int2(  1, -1 ), int2( 2, -1 ), int2( 3, -1 ), 
        int2( -3,  0 ), int2( -2,  0 ), int2( -1,  0 ), int2(  0,  0 ), int2(  1,  0 ), int2( 2,  0 ), int2( 3,  0 ), 
        int2( -3,  1 ), int2( -2,  1 ), int2( -1,  1 ), int2(  0,  1 ), int2(  1,  1 ), int2( 2,  1 ), int2( 3,  1 ), 
        int2( -3,  2 ), int2( -2,  2 ), int2( -1,  2 ), int2(  0,  2 ), int2(  1,  2 ), int2( 2,  2 ), int2( 3,  2 ),
        int2( -3,  3 ), int2( -2,  3 ), int2( -1,  3 ), int2(  0,  3 ), int2(  1,  3 ), int2( 2,  3 ), int2( 3,  3 ) 
    };

    float2 rt_dimensions;
    depth.GetDimensions( rt_dimensions.x, rt_dimensions.y );

    float depth_ref = LinearDepth( depth[thread.xy].r );

    float sum = 0.0f;
    float weight_sum = 0.0f;
    [unroll]
    for ( int i = 0; i < noffsets; ++i )
    {
        float2 texcoord = float2( thread.xy + offsets[i] ) / rt_dimensions;
        float sample_depth = LinearDepth( depth[thread.xy + offsets[i]].r );

        float depth_weight = rcp( pow( 2, 50 * abs( sample_depth - depth_ref ) ) );
        sum += input.SampleLevel( linear_wrap_sampler, texcoord, 0 ).r * depth_weight;
        weight_sum += depth_weight;
    }

    output[thread.xy] = min16float( saturate( sum / weight_sum ) );
}