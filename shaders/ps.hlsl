#include "lib/lighting.hlsli"
#include "lib/shadows.hlsli"

#define PER_MATERIAL_CB_BINDING b1
#include "bindings/material_cb.hlsli"

#define PER_PASS_CB_BINDING b2
#include "bindings/pass_cb.hlsli"

struct IBLTransform
{
    float4x4 world2env_mat;
    float4x4 world2env_inv_transposed_mat;
};

cbuffer cbIBLTransform : register(b3)
{
    IBLTransform ibl;
}

cbuffer cbIBLParams : register(b4)
{
    float ibl_radiance_multiplier;
}

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
Texture2DArray shadow_cascade : register(t4);
Texture2D brdf_lut : register(t5);
TextureCube irradiance_map : register(t6);
TextureCube reflection_probe : register(t7);

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
	float lambert_term = saturate( dot( to_source, normal ) );
	float normal_to_eye_cos = saturate( dot( to_camera, normal ) );
	float3 h = normalize( halfvector( to_source, to_camera ) );
	float source_to_half_cos = saturate( dot( to_source, h ) );

	float3 diffuse_albedo = (1.0f - metallic) * base_color.rgb;
	float3 fresnel_r0 = lerp( material.diffuse_fresnel, base_color.rgb, metallic );

    float3 fresnel_term_d = float3( 1.0f, 1.0f, 1.0f ) - fresnel_schlick( fresnel_r0, lambert_term );

	return ( lambert_term )
		    * ( fresnel_term_d * diffuse_disney( roughness, lambert_term, normal_to_eye_cos, source_to_half_cos ) * diffuse_albedo +
               bsdf_ggx_optimized( fresnel_r0, dot( normal, h ), source_to_half_cos, normal_to_eye_cos, lambert_term, roughness ) );
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

float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1,1,1) * (1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}


PixelOut main(PixelIn pin)
{
    float4 base_color = base_color_map.Sample( anisotropic_wrap_sampler, pin.uv );
    base_color = pow( base_color, 2.2f );

#ifdef DEBUG_TEXTURE_LOD
    float lod = base_color_map.CalculateLevelOfDetail( anisotropic_wrap_sampler, pin.uv );

    float4 oversampled_color = float4( 1, 1, 0.1, 1 );
    float4 undersampled_color = float4( 1, 0.1, 1, 1 );

    if ( lod > 2.0f )
    {
        // oversampled
        base_color = lerp( base_color, oversampled_color, pow( (clamp( lod, 2.0f, 4.0f ) - 2.0f) / 2.0f, 2.2f ) );
    }
    else if ( lod < 0.1f )
    {
        // undersampled
        base_color = lerp( base_color, undersampled_color, pow( 1.0f - lod * 10.0f, 2.2f ) );
    }
#endif

	float3 res_color = float3( 0.0f, 0.0f, 0.0f );

    float tangent_normal_z;
    pin.normal = normalize( pin.normal );
    pin.tangent = normalize( pin.tangent - dot( pin.tangent, pin.normal ) * pin.normal );
    pin.binormal = cross( pin.normal, pin.tangent );
	float3 normal = ws_normal_bump_mapping( normalize( pin.normal ),
											pin.tangent,
											pin.binormal,
											normal_map.Sample( linear_wrap_sampler, pin.uv ).xy,
                                            tangent_normal_z );

	float3 specular = specular_map.Sample( linear_wrap_sampler, pin.uv ).rgb; // r - occlusion, g - smoothness, b - metallic

	for ( int light_idx = 0; light_idx < pass_params.n_parallel_lights; ++light_idx )
	{
        ParallelLight light = pass_params.parallel_lights[light_idx];

		float3 light_radiance = rendering_equation( base_color, light.dir,
												 normalize( -pin.pos_v ),
												 normal,
												 1.0f - specular.g, specular.b );

        res_color += light.strength
                     * light_radiance * csm_shadow_factor( pin.pos_v, light, shadow_cascade, pass_params.csm_split_positions, shadow_map_sampler );
    }

    // ambient for sky, remove after skybox gen
    const float3 ambient_color_linear = float3(0.0558f, 0.078f, 0.138f);
  
    float4 irradiance_dir = mul( mul( float4( normal, 0.0f ), pass_params.view_inv_mat ), ibl.world2env_mat );
    float4 reflection_dir = mul( mul( float4( reflect( pin.pos_v, normal ), 0.0f ), pass_params.view_inv_mat ), ibl.world2env_mat );
    PixelOut res;
    res.color = float4(res_color, 1.0f);
    
    //res.ambient_color = float4( percieved_brightness( pass_params.parallel_lights[0].strength ) * base_color.rgb * ambient_color_linear, 1.0f);
    res.ambient_color = float4( (1.0f - specular.b) * ibl_radiance_multiplier * irradiance_map.Sample( linear_wrap_sampler, irradiance_dir.xyz ).xyz * base_color.rgb, 1.0f);

    float metallic = specular.b;
	float3 diffuse_albedo = (1.0f - metallic) * base_color.rgb;
	float3 fresnel_r0 = lerp( material.diffuse_fresnel, base_color.rgb, metallic );
    float cos_nv = saturate( dot( normalize( -pin.pos_v ), normal ) );
    float roughness = 1.0f - specular.g;
    //fresnel_r0 = fresnelSchlickRoughness( cos_nv, fresnel_r0, roughness );
    float3 prefiltered_spec_radiance = reflection_probe.SampleLevel( linear_wrap_sampler, reflection_dir.xyz, lerp( 0.1, 1, roughness ) * 6.0f );
    float2 env_brdf = brdf_lut.Sample( linear_wrap_sampler, float2( saturate( dot( normalize( -pin.pos_v ), normal ) ), roughness ) ).xy;

    float3 specular_ambient = ibl_radiance_multiplier * prefiltered_spec_radiance * ( fresnel_r0 * env_brdf.x + fresnelSchlickRoughness( 0, fresnel_r0, roughness ) * env_brdf.y );

    res.ambient_color += float4( specular_ambient, 0.0f );

    res.screen_space_normal = normal.xy;
	return res;
}