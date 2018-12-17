// Dimensions for output and depth textures must match

#define PER_PASS_CB_BINDING b0
#include "../bindings/pass_cb.hlsli"

cbuffer cbSettings : register( b1 )
{
    bool transposed;
};

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
    const int noffsets = 7;
    int2 offsets[noffsets] =
    {
        int2( -3,  0 ), int2( -2,  0 ), int2( -1,  0 ), int2(  0,  0 ), int2(  1,  0 ), int2( 2,  0 ), int2( 3,  0 )
    };

    float2 rt_dimensions;
    if ( transposed )
        depth.GetDimensions( rt_dimensions.y, rt_dimensions.x );
    else
        depth.GetDimensions( rt_dimensions.x, rt_dimensions.y );
    
    uint2 ref_coord = transposed ? thread.yx : thread.xy;

    float depth_ref = LinearDepth( depth[ref_coord].r );

    float sum = 0.0f;
    float weight_sum = 0.0f;
    [unroll]
    for ( int i = 0; i < noffsets; ++i )
    {
        float sample_depth = LinearDepth( depth[ref_coord + ( transposed ? offsets[i].yx : offsets[i].xy )].r );
        
        float depth_weight = rcp( pow( 2, 50 * abs( sample_depth - depth_ref ) ) );

        float2 texcoord = float2( thread.xy + offsets[i] ) / rt_dimensions;
        sum += input.SampleLevel( linear_wrap_sampler, texcoord, 0 ).r * depth_weight;
        weight_sum += depth_weight;
    }

    output[thread.yx] = min16float( saturate( sum / weight_sum ) );
}