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
    return ( ( near_z / ( near_z - far_z ) ) * far_z ) / ( hyperbolic_z - ( far_z / ( far_z - near_z ) ) );
}

float3 CalcSampleOffsetCoord( float2 offset_multiplied_by_ws_radius, float2 origin_texcoord, float origin_depth )
{
    float2 uv_offset = offset_multiplied_by_ws_radius / ( origin_depth * fov_y );
    uv_offset *= float2( 1.0f / aspect_ratio, -1.0f );

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

    const float pi_div_2 = 3.1415f / 2.0f;
    const float angle_bias = settings.angle_bias;
    
    const int nsamples = settings.nsamples_per_direction;
    const float max_r = settings.max_r;

    float occlusion = 0.0f;

    [unroll]
    for ( int dir = 0; dir < ndirs; ++dir )
    {
        float max_horizon = angle_bias;
        for ( int isample = 1; isample <= nsamples; isample++ )
        {
            float2 offset_in_ws = offsets[dir] * ( float( isample ) * max_r / float( nsamples ) );
            float3 diff = CalcSampleOffsetCoord( offset_in_ws, target_texcoord, linear_depth );
            if ( abs( diff.z ) < max_r )
                max_horizon = max( max_horizon, CalcHorizonAngle( diff, view_normal ) );
        }

        occlusion += max( max_horizon - angle_bias, 0 ) / ( pi_div_2 - angle_bias );
    }

    occlusion /= ndirs;

    return 1.0f - occlusion;
}