#ifndef LIGHTING_HLSLI
#define LIGHTING_HLSLI

#include "math_utils.hlsli"

struct Light
{
	float4x4 shadow_map_mat;
	float3 strength;
	float falloff_start;
	float3 origin;
	float falloff_end;
	float3 dir;
	float spot_power;
};

#define MAX_CASCADE_SIZE 4
struct ParallelLight
{
	float4x4 shadow_map_mat[MAX_CASCADE_SIZE];
    float3 strength;
    int csm_num_split_positions;
	float3 dir;
    float _padding;
};

static const float M_PI = 3.14159265f;

// all outputs are not normalized
float3 halfvector( float3 to_source, float3 to_camera )
{
	return to_source + to_camera;
}

float lambert( float3 to_source, float3 normal )
{
	return dot( normal, to_source );
}

float3 fresnel_schlick( float3 fresnel_r0, float source_to_normal_angle_cos )
{
	return fresnel_r0 + ( float3( 1.0f, 1.0f, 1.0f ) - fresnel_r0 ) * pow( 1.0f - source_to_normal_angle_cos, 5 );
}

float diffuse_disney( float roughness, float source_to_normal_cos, float normal_to_eye_cos, float source_to_half_cos )
{
	// disney semi-pbr diffuse model
	float f = sqr( source_to_half_cos ) * roughness * 2.0f - 0.5f;

	return ( 1.0f + f * pow( 1 - source_to_normal_cos, 5 ) )
		* ( 1.0f + f * pow( 1 - normal_to_eye_cos, 5 ) ) / M_PI;
}

// GGX self-shadowing term with Smith's method, cos_nv = ( n, v ), where v is view vector or light vector , alpha = roughness^2
float smith_ggx_shadowing( float cos_nv, float alpha2 )
{
    float denominator = cos_nv + sqrt( lerp( sqr( cos_nv ), 1.0f, alpha2 ) );
    return 2.0f * cos_nv / denominator;
}

// correct only if lambert term is above 0 and triangle is facing camera
float3 bsdf_ggx( float3 fresnel_r0, float cos_nh, float cos_lh, float cos_nv, float cos_nl, float roughness )
{
	// ggx ndf, shlick approximation for fresnel
	// alpha remapped to [0.5, 1]
	roughness = sqr( lerp( 0.5, 1, roughness ) );
	float alpha2 = sqr( roughness );
	float ndf = alpha2 / ( M_PI * sqr( ( alpha2 - 1 ) * sqr( cos_nh ) + 1 ) );

    // Smith's method for selfshadowing
    float shadowing = smith_ggx_shadowing( cos_nv, alpha2 ) * smith_ggx_shadowing( cos_nl, alpha2 );
    float brdf_denominator = 4 * cos_nl * cos_nv;

	return saturate( fresnel_schlick( fresnel_r0, cos_lh ) * ndf * shadowing / brdf_denominator );

}

float3 bsdf_ggx_optimized( float3 fresnel_r0, float cos_nh, float cos_lh, float cos_nv, float cos_nl, float roughness )
{
	roughness = sqr( lerp( 0.5, 1, roughness ) );
	float alpha2 = sqr( roughness );
    float one_minus_alpha2 = 1.0f - alpha2;
    float ndf_denom = M_PI * sqr( sqr( cos_nh ) * ( -one_minus_alpha2 ) + 1.0f );
    float g_l_denom = sqrt( alpha2 + one_minus_alpha2 * sqr( cos_nl ) ) + cos_nl;
    float g_v_denom = sqrt( alpha2 + one_minus_alpha2 * sqr( cos_nv ) ) + cos_nv;
    float bsdf_no_fresnel = alpha2 * rcp( ndf_denom * g_l_denom * g_v_denom );
    return saturate( fresnel_schlick( fresnel_r0, cos_lh ) * bsdf_no_fresnel );
}



#endif // LIGHTING_HLSLI