#include "lib/lighting.hlsli"

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
	float3 pos_w : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
	float2 uv : TEXCOORD;
};

float3 rendering_equation( float3 to_source, float3 to_camera, float3 normal, float2 uv )
{
	float lambert_term = max( dot( to_source, normal ), 0 );
	float normal_to_eye_cos = max( dot( to_camera, normal ), 0 );
	float3 h = normalize( halfvector( to_source, to_camera ) );
	float source_to_half_cos = max( dot( to_source, h ), 0 );

	float4 base_color = base_color_map.Sample( anisotropic_wrap_sampler, uv );
    base_color.r = pow(base_color.r, 2.2f);
    base_color.g = pow(base_color.g, 2.2f);
    base_color.b = pow(base_color.b, 2.2f);

	// alpha test
	clip( base_color.a - 0.01 );

	float3 specular = specular_map.Sample( anisotropic_wrap_sampler, uv ).rgb; // r - occlusion, g - roughness, b - metallic
	float roughness = specular.g;
	float metallic = specular.b;

	float3 diffuse_albedo = (1.0f - metallic) * base_color.rgb;
	float3 fresnel_r0 = lerp( diffuse_fresnel, base_color.rgb, metallic );

	return ( lambert_term )
		    * ( diffuse_disney( roughness, lambert_term, normal_to_eye_cos, source_to_half_cos ) * diffuse_albedo 
				+ specular_strength( fresnel_r0,
									 dot( normal, h ),
									 source_to_half_cos,
									 h,
									 roughness ) );
}

float3 ws_normal_bump_mapping( float3 ws_normal, float3 ws_tangent, float3 ws_binormal, float2 ts_normal_compressed, out float tangent_normal_z )
{
	ts_normal_compressed = 2 * ts_normal_compressed - float2( 1.0f, 1.0f );
	tangent_normal_z = sqrt( 1.0 - pow( ts_normal_compressed.x, 2 ) - pow( ts_normal_compressed.y, 2 ) );
	return normalize( ts_normal_compressed.x * ws_tangent + ts_normal_compressed.y * ws_binormal + tangent_normal_z * ws_normal );
}

float PercievedBrightness(float3 color)
{
    return (0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b);
}

float4 main( PixelIn pin ) : SV_TARGET
{
	float3 res_color = float3( 0.0f, 0.0f, 0.0f );

    float tangent_normal_z;
	float3 normal = ws_normal_bump_mapping( normalize( pin.normal ),
											normalize( pin.tangent ),
											normalize( pin.binormal ),
											normal_map.Sample( linear_wrap_sampler, pin.uv ).xy,
                                            tangent_normal_z );

    float local_ambient_shadowing = 4.0f*((0.5f - acos(tangent_normal_z) * 3.1415f) * 2.0f * 0.2f + 0.8f);

    float3 base_color = base_color_map.Sample(anisotropic_wrap_sampler, pin.uv);

    base_color.r = pow(base_color.r, 2.2f);
    base_color.g = pow(base_color.g, 2.2f);
    base_color.b = pow(base_color.b, 2.2f);

	for ( int light_idx = 0; light_idx < n_parallel_lights; ++light_idx )
	{
		float3 light_radiance = rendering_equation( lights[light_idx].dir,
												 normalize( eye_pos_w - pin.pos_w ),
												 normal,
												 pin.uv );

        res_color += lights[light_idx].strength
                     * (light_radiance * shadow_factor( pin.pos_w, lights[light_idx].shadow_map_mat, shadow_map, shadow_map_sampler )
                            + base_color * pow(0.06f, 2.2f) * local_ambient_shadowing); // second component is bouncing light approximation, remove after GI support
    }

    // ambient for sky, remove after skybox gen
    res_color += PercievedBrightness(lights[0].strength) * base_color * float3(pow(0.06f, 2.2f), pow(0.07f, 2.2f), pow(0.09f, 2.2f)) * local_ambient_shadowing;

	return float4( res_color, 1.0f );
}