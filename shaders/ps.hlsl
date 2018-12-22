#include "lib/lighting.hlsli"
#include "lib/shadows.hlsli"

#define PER_MATERIAL_CB_BINDING b1
#include "bindings/material_cb.hlsli"

#define PER_PASS_CB_BINDING b2
#include "bindings/pass_cb.hlsli"

SamplerState point_wrap_sampler : register(s0);
SamplerState point_clamp_sampler : register(s1);
SamplerState linear_wrap_sampler : register(s2);
SamplerState linear_clamp_sampler : register(s3);
SamplerState anisotropic_wrap_sampler : register(s4);
SamplerState anisotropic_clamp_sampler : register(s5);
SamplerComparisonState shadow_map_sampler : register(s6);

Texture2D base_color_map : register(t0);
Texture2D normal_map : register(t1);
Texture2D specular_map : register(t2);
Texture2D shadow_map : register(t3);

struct PixelIn
{
	float4 pos : SV_POSITION;
	float3 pos_v : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
	float2 uv : TEXCOORD;
};

struct PixelOut
{
    float4 color : SV_TARGET0;
    float4 ambient_color : SV_TARGET1;
    float2 screen_space_normal : SV_TARGET2;
};

float3 rendering_equation( half4 base_color, float3 to_source, float3 to_camera, float3 normal, float roughness, float metallic )
{
	float lambert_term = max( dot( to_source, normal ), 0 );
	float normal_to_eye_cos = max( dot( to_camera, normal ), 0 );
	float3 h = normalize( halfvector( to_source, to_camera ) );
	float source_to_half_cos = max( dot( to_source, h ), 0 );

	float3 diffuse_albedo = (1.0f - metallic) * base_color.rgb;
	float3 fresnel_r0 = lerp( material.diffuse_fresnel, base_color.rgb, metallic );

	return ( lambert_term )
		    * ( diffuse_disney( roughness, lambert_term, normal_to_eye_cos, source_to_half_cos ) * diffuse_albedo 
				+ bsdf_ggx_optimized( fresnel_r0,
							          dot( normal, h ),
							          source_to_half_cos,
                                      normal_to_eye_cos,
                                      lambert_term,
							          roughness ) );
}

float3 ws_normal_bump_mapping( float3 ws_normal, float3 ws_tangent, float3 ws_binormal, float2 ts_normal_compressed, out float tangent_normal_z )
{
	ts_normal_compressed = 2 * ts_normal_compressed - float2( 1.0f, 1.0f );
	tangent_normal_z = sqrt( 1.0 - sqr( ts_normal_compressed.x ) - sqr( ts_normal_compressed.y ) );
	return ts_normal_compressed.x * ws_tangent + ts_normal_compressed.y * ws_binormal + tangent_normal_z * ws_normal;
}

float percieved_brightness(float3 color)
{
    return (0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b);
}

PixelOut main(PixelIn pin)
{
    float4 base_color = base_color_map.Sample( anisotropic_wrap_sampler, pin.uv );

#ifdef DEBUG_TEXTURE_LOD
    float lod = base_color_map.CalculateLevelOfDetail( anisotropic_wrap_sampler, pin.uv );
    if ( lod > 1.0f )
    {
        base_color.r = pow(base_color.r, 2.2f);
        base_color.g = pow(base_color.g, 2.2f);
        base_color.b = pow(base_color.b, 2.2f);
    }
    else
    {
        base_color.r = pow(lod, 2.2f);
        base_color.g = pow(lod, 2.2f);
        base_color.b = pow(lod, 2.2f);
    }
#else
    base_color.r = pow(base_color.r, 2.2f);
    base_color.g = pow(base_color.g, 2.2f);
    base_color.b = pow(base_color.b, 2.2f);
#endif

	float3 res_color = float3( 0.0f, 0.0f, 0.0f );

    float tangent_normal_z;
	float3 normal = ws_normal_bump_mapping( normalize( pin.normal ),
											normalize( pin.tangent ),
											normalize( pin.binormal ),
											normal_map.Sample( linear_wrap_sampler, pin.uv ).xy,
                                            tangent_normal_z );

	float3 specular = specular_map.Sample( linear_wrap_sampler, pin.uv ).rgb; // r - occlusion, g - roughness, b - metallic

	for ( int light_idx = 0; light_idx < pass_params.n_parallel_lights; ++light_idx )
	{
		float3 light_radiance = rendering_equation( base_color, pass_params.lights[light_idx].dir,
												 normalize( - pin.pos_v ),
												 normal,
												 specular.g, specular.b );

        res_color += pass_params.lights[light_idx].strength
                     * light_radiance * shadow_factor( pin.pos_v, pass_params.lights[light_idx].shadow_map_mat, shadow_map, shadow_map_sampler );
    }

    // ambient for sky, remove after skybox gen
    const float3 ambient_color_linear = float3(0.0558f, 0.078f, 0.138f);

    PixelOut res;
    res.color = float4(res_color, 1.0f);
    res.ambient_color = float4( percieved_brightness( pass_params.lights[0].strength ) * base_color.rgb * ambient_color_linear, 1.0f);
    res.screen_space_normal = normal.xy;
	return res;
}