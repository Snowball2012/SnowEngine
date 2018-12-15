#define PER_PASS_CB_BINDING b0
#include "../bindings/pass_cb.hlsli"

#include "../lib/math_utils.hlsli"

struct HBAOSettings
{
    float2 render_target_size; 
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


float Occlusion( float3 origin2sample, float3 normal, float max_r )
{
    float horizon_occlusion = saturate( dot( normalize( origin2sample ), normal ) );

    // reset occlusion to 0 without branching if distance to the sample is too big;
    return min( horizon_occlusion, ( max_r * max_r ) > dot( origin2sample, origin2sample ) );
}

float4 ReconstructPositionVS( float2 uv )
{
    float4 ndc = float4( uv, hyperbolic_depth_map.Sample( linear_wrap_sampler, uv ).r, 1.0f );
    ndc.xy *= 2.0f;
    ndc.xy = float2( ndc.x - 1.0f, 1.0f - ndc.y );

    float4 vs_coord = mul( ndc, pass_params.proj_inv_mat );
    vs_coord.xyz /= vs_coord.w;

    return vs_coord;
}

float3 ReconstructNormalVS( float2 uv )
{
    float3 normal_vs;
    normal_vs.xy = normal_map.Sample( linear_wrap_sampler, uv ).xy;
    normal_vs.z = -sqrt( 1.0h - sqr( normal_vs.x ) - sqr( normal_vs.y ) );

    return normal_vs;
}

float2 SampleStepUV( float vs_step, float distance_to_camera )
{
    float2 sample_step_uv = float2( rcp( pass_params.aspect_ratio ), -1.0h );
    sample_step_uv *= vs_step * rcp( max( distance_to_camera * pass_params.fov_y, 10 ) );

    return sample_step_uv;
}

// produces sine and cosine of a random rotation
float2 RandomRotation( float2 seed )
{
    float2 rotation;
    sincos( abs(seed.x + seed.y) * M_PI / 2.0f , rotation.x, rotation.y );
    return rotation;
}

float2 RotateDirection( float2 direction, float2 rotation )
{
    return float2( dot( rotation, direction ), rotation.x * direction.y - rotation.y * direction.x );
}

float BiasedOcclusion( float occlusion, float bias )
{
    return max( occlusion - bias, 0.0h ) * rcp( 1.0f - bias );
}

float main( float4 coord : SV_POSITION ) : SV_TARGET
{
    float2 origin_uv = coord.xy / settings.render_target_size;

    float2 seed = (frac(sin(dot(origin_uv ,float2(12.9898,78.233)*2.0)) * 43758.5453)); // randomization for marching directions & pixel position jittering

    // origin sample setup
    origin_uv += seed / ( 2.0f * settings.render_target_size );

    float3 origin_vs = ReconstructPositionVS( origin_uv ).xyz;
    float3 normal_vs = ReconstructNormalVS( origin_uv );

    // direction marching setup
    const int ndirs = 4;
    float2 offsets[ndirs] =
    {
        float2( -1.0f, 0.0f ),
        float2( 0.0f, 1.0f ),
        float2( 1.0f, 0.0f ),
        float2( 0.0f, -1.0f )
    };

    const float pi_div_2 = M_PI / 2.0f;
    const float occlusion_bias = settings.angle_bias / pi_div_2;
    
    const float max_r = settings.max_r;
    const int nsamples = settings.nsamples_per_direction;
    
    float2 sample_step_uv = SampleStepUV( max_r * rcp( float( nsamples ) ), length( origin_vs ) );
    float2 rotation = RandomRotation( seed );

    float occlusion = 0.0f;
    [unroll]
    for ( int dir = 0; dir < ndirs; ++dir )
    {
        float2 offset_dir_uv = RotateDirection( offsets[dir], rotation );
        offset_dir_uv *= sample_step_uv;

        float max_occlusion = occlusion_bias;

        for ( int isample = 1; isample <= nsamples; isample++ )
        {
            float3 diff_vs = ReconstructPositionVS( origin_uv + offset_dir_uv * isample ).xyz - origin_vs;
            float sample_occlusion = Occlusion( diff_vs, normal_vs, max_r );
            max_occlusion = max( max_occlusion, sample_occlusion );
        }

        occlusion += BiasedOcclusion( max_occlusion, occlusion_bias );
    }

    occlusion /= ndirs;

    return 1.0h - occlusion;
}