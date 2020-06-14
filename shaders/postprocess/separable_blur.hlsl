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

float linear_depth( float hyperbolic_z )
{
    float remapped_z = 2.0 * hyperbolic_z - 1.0;
    return 2.0 * pass_params.near_z * pass_params.far_z / (pass_params.far_z + pass_params.near_z - remapped_z * (pass_params.far_z - pass_params.near_z));
}

static const int BLUR_RADIUS = 10;
static const float SIGMA = BLUR_RADIUS;

static const int GROUP_SIZE_X = 256; // optimal for 4k blur on NVidia GeForce GTX 1080ti
static const int GROUP_SIZE_Y = 1;
static const int GROUP_DATA_ELEM_NUM_X = BLUR_RADIUS * 2 + GROUP_SIZE_X;

groupshared float shared_depth_data[GROUP_DATA_ELEM_NUM_X * GROUP_SIZE_Y];
groupshared float shared_ssao_data[GROUP_DATA_ELEM_NUM_X * GROUP_SIZE_Y];

[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void main( uint3 thread : SV_DispatchThreadID, uint3 thread_in_group : SV_GroupThreadID )
{
    float2 rt_dimensions;
    uint2 ref_coord;
    int2 offset;
    if ( transposed )
    {
        depth.GetDimensions( rt_dimensions.y, rt_dimensions.x );
        ref_coord = thread.yx;
        offset = int2( 0, 1 );
    }
    else
    {
        depth.GetDimensions( rt_dimensions.x, rt_dimensions.y );
        ref_coord = thread.xy;
        offset = int2( 1, 0 );
    }

    // 1. Collect group data
    uint group_data_idx = thread_in_group.y * GROUP_DATA_ELEM_NUM_X + BLUR_RADIUS + thread_in_group.x;
    
    float depth_ref = linear_depth( depth[ref_coord].r );

    shared_depth_data[group_data_idx] = depth_ref;
    shared_ssao_data[group_data_idx] = input.SampleLevel( linear_wrap_sampler, float2( thread.xy ) / rt_dimensions, 0 ).r;

    int is_hi_thread_in_group = ( (int)thread_in_group.x >= ( GROUP_SIZE_X - BLUR_RADIUS ) );
    if ( ( (int)thread_in_group.x < BLUR_RADIUS ) + is_hi_thread_in_group )
    {
        int additional_offset =  -BLUR_RADIUS + is_hi_thread_in_group * 2 * BLUR_RADIUS;
        uint additional_data_idx = group_data_idx + additional_offset;
        shared_depth_data[additional_data_idx] = linear_depth( depth[ref_coord + additional_offset * offset].r );
        shared_ssao_data[additional_data_idx] = input.SampleLevel( linear_wrap_sampler, float2( thread.xy + int2( additional_offset, 0 ) ) / rt_dimensions, 0 ).r;
    }

    GroupMemoryBarrierWithGroupSync();

    // 2. Blur
    float sum = 0.0f;
    float weight_sum = 0.0f;
    [unroll]
    for ( int i = -BLUR_RADIUS; i <= BLUR_RADIUS; ++i )
    {
        float sample_depth = shared_depth_data[group_data_idx + i];
        float weight = rcp( pow( 2, 40 * abs( sample_depth - depth_ref ) ) ) * exp( - i * i / ( 2.0f * ( SIGMA * SIGMA ) ) ); // bilateral depth * gauss

        sum += shared_ssao_data[group_data_idx + i] * weight;
        weight_sum += weight;
    }

    output[thread.yx] = saturate( min16float( sum / weight_sum ) );
}