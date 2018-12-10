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
min16float GetEyeDepth( min16float hyperbolic_z, min16float2 depth_tf )
{
    return depth_tf.x / ( hyperbolic_z - depth_tf.y );
}

min16float3 CalcSampleOffsetCoord( min16float2 offset_multiplied_by_ws_radius, min16float2 origin_texcoord, min16float origin_depth, min16float2 depth_tf )
{
    min16float2 uv_offset = offset_multiplied_by_ws_radius * rcp( origin_depth * pass_params.fov_y );
    uv_offset *= min16float2( rcp( pass_params.aspect_ratio ), -1.0h );

    min16float sample_depth = GetEyeDepth( hyperbolic_depth_map.Sample( point_wrap_sampler, origin_texcoord + uv_offset ).r, depth_tf );

    return min16float3( offset_multiplied_by_ws_radius, sample_depth - origin_depth );
}

min16float CalcHorizonAngle( min16float3 diff, min16float3 normal )
{
    return ( 3.14h / 2.0h ) - acos( dot( normalize( diff ), normal ) );
}

min16float main( float4 coord : SV_POSITION ) : SV_TARGET
{
    min16float2 depth_tex_size;
    hyperbolic_depth_map.GetDimensions( depth_tex_size.x, depth_tex_size.y );
    min16float2 target_texcoord = coord.xy / depth_tex_size;

    min16float depth_tf_numerator_part = pass_params.near_z / ( pass_params.near_z - pass_params.far_z );
    min16float2 depth_tf = min16float2( depth_tf_numerator_part * pass_params.far_z, 1.0h - depth_tf_numerator_part );

    min16float linear_depth = GetEyeDepth( hyperbolic_depth_map.Sample( point_wrap_sampler, target_texcoord ).r, depth_tf );
    min16float3 view_normal = min16float3( normal_map.Sample( point_wrap_sampler, target_texcoord ).xy, 0 );
    view_normal.z = -sqrt( 1.0h - sqr( view_normal.x ) - sqr( view_normal.y ) );

    const int ndirs = 8;
    min16float2 offsets[ndirs] = { min16float2( -1.0f, 0.0f ),
                          min16float2( 0.0f, 1.0f ),
                          min16float2( 1.0f, 0.0f ),
                          min16float2( 0.0f, -1.0f ), 
                          min16float2( -0.7f, 0.7f ),
                          min16float2( 0.7f, 0.7f ),
                          min16float2( 0.7f, -0.7f ),
                          min16float2( -0.7f, -0.7f ) };

    const int nrotations = 8;
    min16float2 rotations[nrotations] = {
        min16float2( 0, 1 ),
        min16float2( sin( M_PI / ( 8.0f * nrotations )* 1.0f ), cos( M_PI / ( 8.0f * nrotations )* 1.0f ) ),
        min16float2( sin( M_PI / ( 8.0f * nrotations )* 2.0f ), cos( M_PI / ( 8.0f * nrotations )* 2.0f ) ),
        min16float2( sin( M_PI / ( 8.0f * nrotations )* 3.0f ), cos( M_PI / ( 8.0f * nrotations )* 3.0f ) ),
        min16float2( sin( M_PI / ( 8.0f * nrotations )* 4.0f ), cos( M_PI / ( 8.0f * nrotations )* 4.0f ) ),
        min16float2( sin( M_PI / ( 8.0f * nrotations )* 5.0f ), cos( M_PI / ( 8.0f * nrotations )* 5.0f ) ),
        min16float2( sin( M_PI / ( 8.0f * nrotations )* 6.0f ), cos( M_PI / ( 8.0f * nrotations )* 6.0f ) ),
        min16float2( sin( M_PI / ( 8.0f * nrotations )* 7.0f ), cos( M_PI / ( 8.0f * nrotations )* 7.0f ) ),
    };


    const min16float pi_div_2 = M_PI / 2.0f;
    const min16float angle_bias = settings.angle_bias;
    
    const int nsamples = settings.nsamples_per_direction;
    const min16float max_r = settings.max_r;

    min16float occlusion = 0.0f;

    int rotation = ( ( asuint( 34.845732f * coord.x + 168.0321341f * coord.y ) ) ) % nrotations;

    [unroll]
    for ( int dir = 0; dir < ndirs; ++dir )
    {
        min16float2 offset_dir_ss = offsets[dir];
        offset_dir_ss = min16float2( dot( rotations[rotation], offset_dir_ss ), rotations[rotation].x * offset_dir_ss.y - rotations[rotation].y * offset_dir_ss.x );
        min16float max_horizon = angle_bias;
        for ( int isample = 1; isample <= nsamples; isample++ )
        {
            min16float2 offset_in_ws = offset_dir_ss * ( min16float( isample ) * max_r / min16float( nsamples ) ); 
            min16float3 diff = CalcSampleOffsetCoord( offset_in_ws, target_texcoord, linear_depth, depth_tf );
            max_horizon = max( min( max( max_horizon, CalcHorizonAngle( diff, view_normal ) ), ( max_r - abs( diff.z ) ) * 1.e5h ), 0.0h );
        }

        occlusion += max( max_horizon - angle_bias, 0.0h ) / ( pi_div_2 - angle_bias );
    }

    occlusion /= ndirs;

    return 1.0h - occlusion;
}