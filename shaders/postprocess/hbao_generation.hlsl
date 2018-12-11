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


// full formula is:
// linear_depth = ( ( pass_params.near_z / ( pass_params.near_z - pass_params.far_z ) ) * pass_params.far_z )
//                / ( hyperbolic_z - ( pass_params.far_z / ( pass_params.far_z - pass_params.near_z ) ) );
//
// numerator = ( ( pass_params.near_z / ( pass_params.near_z - pass_params.far_z ) ) * pass_params.far_z )
// denominator_part = ( pass_params.far_z / ( pass_params.far_z - pass_params.near_z )
// they are packed together in depth_tf
float GetEyeDepth( float hyperbolic_z, float2 depth_tf )
{
    return depth_tf.x / ( hyperbolic_z - depth_tf.y );
}

float3 CalcSampleOffsetCoord( float2 offset_multiplied_by_ws_radius, float2 origin_texcoord, float origin_depth, float2 depth_tf )
{
    float2 uv_offset = offset_multiplied_by_ws_radius * rcp( max( origin_depth * pass_params.fov_y, 10 ) );
    uv_offset *= float2( rcp( pass_params.aspect_ratio ), -1.0h );

    float sample_depth = GetEyeDepth( hyperbolic_depth_map.Sample( point_wrap_sampler, origin_texcoord + uv_offset ).r, depth_tf );

    return float3( offset_multiplied_by_ws_radius, sample_depth - origin_depth );
}

float CalcHorizonAngle( float3 diff, float3 normal )
{
    return ( 3.14h / 2.0h ) - acos( dot( normalize( diff ), normal ) );
}

float main( float4 coord : SV_POSITION ) : SV_TARGET
{
    float2 depth_tex_size;
    hyperbolic_depth_map.GetDimensions( depth_tex_size.x, depth_tex_size.y );
    float2 target_texcoord = coord.xy / depth_tex_size;

    float depth_tf_numerator_part = pass_params.near_z * rcp( pass_params.near_z - pass_params.far_z );
    float2 depth_tf = float2( depth_tf_numerator_part * pass_params.far_z, 1.0h - depth_tf_numerator_part );

    float linear_depth = GetEyeDepth( hyperbolic_depth_map.Sample( point_wrap_sampler, target_texcoord ).r, depth_tf );
    float3 view_normal;
    view_normal.xy = normal_map.Sample( point_wrap_sampler, target_texcoord ).xy;
    view_normal.z = -sqrt( 1.0h - sqr( view_normal.x ) - sqr( view_normal.y ) );

    const int ndirs = 4;
    float2 offsets[ndirs] =
    {
        float2( -1.0f, 0.0f ),
        float2( 0.0f, 1.0f ),
        float2( 1.0f, 0.0f ),
        float2( 0.0f, -1.0f )
    };

    const float pi_div_2 = M_PI / 2.0f;
    const float angle_bias = settings.angle_bias;
    
    const float max_r = settings.max_r;
    const int nsamples = settings.nsamples_per_direction;
    float sample_step_ws = max_r * rcp( float( nsamples ) );

    float occlusion = 0.0f;

    float2 rotation;
    float2 seed = (frac(sin(dot(target_texcoord ,float2(12.9898,78.233)*2.0)) * 43758.5453));
    sincos( abs(seed.x + seed.y) * M_PI / 2.0f , rotation.x, rotation.y );

    [unroll]
    for ( int dir = 0; dir < ndirs; ++dir )
    {
        float2 offset_dir_ss = offsets[dir];
        offset_dir_ss = float2( dot( rotation, offset_dir_ss ), rotation.x * offset_dir_ss.y - rotation.y * offset_dir_ss.x );
        offset_dir_ss *= sample_step_ws;
        float max_horizon = angle_bias;

        for ( int isample = 1; isample <= nsamples; isample++ )
        {
            float2 offset_in_ws = offset_dir_ss * isample; 
            float3 diff = CalcSampleOffsetCoord( offset_in_ws, target_texcoord, linear_depth, depth_tf );
            
            max_horizon = max( max_horizon, min( CalcHorizonAngle( diff, view_normal ), ( max_r > abs( diff.z ) ) * M_PI ) );
        }

        occlusion += max( max_horizon - angle_bias, 0.0h ) * rcp( pi_div_2 - angle_bias );
    }

    occlusion /= ndirs;

    return 1.0h - occlusion;
}