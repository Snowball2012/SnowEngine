#define PER_PASS_CB_BINDING b0
#include "../bindings/pass_cb.hlsli"

#include "../lib/math_utils.hlsli"

struct HBAOSettings
{
    float max_r;
    float angle_bias;
    int nsamples_per_direction;
};

cbuffer cbSettings : register( b1 )
{
    HBAOSettings settings;
};

SamplerState linear_wrap_sampler : register( s0 );
SamplerState point_wrap_sampler : register( s1 );

Texture2D hyperbolic_depth_map : register( t0 );
Texture2D normal_map : register( t1 );

float GetEyeDepth( float hyperbolic_z )
{
    return ( ( pass_params.near_z / ( pass_params.near_z - pass_params.far_z ) ) * pass_params.far_z )
           / ( hyperbolic_z - ( pass_params.far_z / ( pass_params.far_z - pass_params.near_z ) ) );
}

float3 CalcSampleOffsetCoord( float2 offset_multiplied_by_ws_radius, float2 origin_texcoord, float origin_depth )
{
    float2 uv_offset = offset_multiplied_by_ws_radius / ( origin_depth * pass_params.fov_y );
    uv_offset *= float2( 1.0f / pass_params.aspect_ratio, -1.0f );

    float sample_depth = GetEyeDepth( hyperbolic_depth_map.Sample( linear_wrap_sampler, origin_texcoord + uv_offset ).r );

    return float3( offset_multiplied_by_ws_radius, sample_depth - origin_depth );
}

float CalcHorizonAngle( float3 diff, float3 normal )
{
    return ( 3.14f / 2 ) - acos( dot( normalize( diff ), normal ) );
}

float main( float4 coord : SV_POSITION ) : SV_TARGET
{
    float2 depth_tex_size;
    hyperbolic_depth_map.GetDimensions( depth_tex_size.x, depth_tex_size.y );
    float2 target_texcoord = coord.xy / depth_tex_size;

    float linear_depth = GetEyeDepth( hyperbolic_depth_map.Sample( point_wrap_sampler, target_texcoord ).r );
    float3 view_normal = float3( normal_map.Sample( point_wrap_sampler, target_texcoord ).xy, 0 );
    view_normal.z = -sqrt( 1.0f - sqr( view_normal.x ) - sqr( view_normal.y ) );

    const int ndirs = 8;
    float2 offsets[ndirs] = { float2( -1.0f, 0.0f ),
                          float2( 0.0f, 1.0f ),
                          float2( 1.0f, 0.0f ),
                          float2( 0.0f, -1.0f ), 
                          float2( -0.7f, 0.7f ),
                          float2( 0.7f, 0.7f ),
                          float2( 0.7f, -0.7f ),
                          float2( -0.7f, -0.7f ) };

    const int nrotations = 8;
    float2 rotations[nrotations] = {
        float2( 0, 1 ),
        float2( sin( M_PI / ( 8.0f * nrotations )* 1.0f ), cos( M_PI / ( 8.0f * nrotations )* 1.0f ) ),
        float2( sin( M_PI / ( 8.0f * nrotations )* 2.0f ), cos( M_PI / ( 8.0f * nrotations )* 2.0f ) ),
        float2( sin( M_PI / ( 8.0f * nrotations )* 3.0f ), cos( M_PI / ( 8.0f * nrotations )* 3.0f ) ),
        float2( sin( M_PI / ( 8.0f * nrotations )* 4.0f ), cos( M_PI / ( 8.0f * nrotations )* 4.0f ) ),
        float2( sin( M_PI / ( 8.0f * nrotations )* 5.0f ), cos( M_PI / ( 8.0f * nrotations )* 5.0f ) ),
        float2( sin( M_PI / ( 8.0f * nrotations )* 6.0f ), cos( M_PI / ( 8.0f * nrotations )* 6.0f ) ),
        float2( sin( M_PI / ( 8.0f * nrotations )* 7.0f ), cos( M_PI / ( 8.0f * nrotations )* 7.0f ) ),
    };


    const float pi_div_2 = M_PI / 2.0f;
    const float angle_bias = settings.angle_bias;
    
    const int nsamples = settings.nsamples_per_direction;
    const float max_r = settings.max_r;

    float occlusion = 0.0f;

    int rotation = ( ( asuint( 34.845732f * coord.x + 168.0321341f * coord.y ) ) >> 5 ) % nrotations;

    [unroll]
    for ( int dir = 0; dir < ndirs; ++dir )
    {
        float2 offset_dir_ss = offsets[dir];
        offset_dir_ss = float2( dot( rotations[rotation], offset_dir_ss ), rotations[rotation].x * offset_dir_ss.y - rotations[rotation].y * offset_dir_ss.x );
        float max_horizon = angle_bias;
        for ( int isample = 1; isample <= nsamples; isample++ )
        {
            float2 offset_in_ws = offset_dir_ss * ( float( isample ) * max_r / float( nsamples ) );
            float3 diff = CalcSampleOffsetCoord( offset_in_ws, target_texcoord, linear_depth );
            if ( abs( diff.z ) < max_r )
                max_horizon = max( max_horizon, CalcHorizonAngle( diff, view_normal ) );
        }

        occlusion += max( max_horizon - angle_bias, 0 ) / ( pi_div_2 - angle_bias );
    }

    occlusion /= ndirs;

    return 1.0f - occlusion;
}