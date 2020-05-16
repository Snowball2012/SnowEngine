#include "lib/lighting.hlsli"
#include "lib/shadows.hlsli"
#include "lib/colorspaces.hlsli"

#define PER_MATERIAL_CB_BINDING b1
#include "bindings/material_cb.hlsli"

#define PER_PASS_CB_BINDING b2
#include "bindings/pass_cb.hlsli"

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
Texture2D brdf_lut : register(t3);

Texture2D shadow_map : register(t4);
Texture2DArray shadow_cascade : register(t5);

TextureCube irradiance_map : register(t6);
TextureCube reflection_probe : register(t7);

struct PixelIn
{
    float4x4 view2env : VIEWTOENV;
    float4 pos : SV_POSITION;
    float3 pos_v : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

struct PixelOut
{
    float4 direct_lighting : SV_TARGET0;
    float4 ambient_lighting : SV_TARGET1;
    float2 screen_space_normal : SV_TARGET2;
};


float3 unpack_normal( float2 compressed_normal, float3 t, float3 b, float3 n )
{
    compressed_normal = 2 * compressed_normal - make_float2( 1.0f );
    float3 compressed_z = sqrt( 1.0 - sqr( compressed_normal.x ) - sqr( compressed_normal.y ) );
    return compressed_normal.x * t + compressed_normal.y * b + compressed_z * n;
}


void unpack_specular( float3 specular_sample, out float roughness, out float metallic )
{
    // specular.a responds to occlusion, but strangely all textures in Bistro scene store 0 there
    roughness = 1.0f - specular_sample.g;
    metallic = specular_sample.b;
}


int csm_get_frustum( float depth, int num_split_positions, float split_positions[MAX_CASCADE_SIZE-1] )
{
    int frustum = 0;
    for ( int i = 0; i < num_split_positions; i++ )
        frustum += depth > split_positions[i]; // no branching, since num_split_positions should be ~[2..4]
    return frustum;
}

float csm_shadow_factor( float3 pos_v, ParallelLight light, Texture2DArray shadow_cascade,
                         float split_positions[MAX_CASCADE_SIZE-1], SamplerComparisonState shadow_map_sampler )
{
    int frustum = csm_get_frustum( pos_v.z, light.csm_num_split_positions, split_positions );
    return shadow_factor_array( pos_v, light.shadow_map_mat[frustum], shadow_cascade, frustum, shadow_map_sampler );
}


float4 dbg_get_lod_color( float4 base_color, float lod )
{
    float4 oversampled_color = float4( 1, 1, 0.1, 1 );
    float4 undersampled_color = float4( 1, 0.1, 1, 1 );

    if ( lod > 2.0f )
    {
        // oversampled
        return lerp( base_color, oversampled_color, ( clamp( lod, 2.0f, 4.0f ) - 2.0f ) / 2.0f );
    }
    else if ( lod < 0.1f )
    {
        // undersampled
        return lerp( base_color, undersampled_color, 1.0f - lod * 10.0f );
    }
    return base_color;
}


PixelOut main(PixelIn pin)
{
    // unpack data
    float4 base_color = base_color_map.Sample( anisotropic_wrap_sampler, pin.uv );

#ifdef DEBUG_TEXTURE_LOD
    float lod = base_color_map.CalculateLevelOfDetail( anisotropic_wrap_sampler, pin.uv );
    base_color = dbg_get_lod_color( base_color, lod );
#endif
    base_color = gamma2linear_std( base_color ) * material.albedo_color;

    float3 bitangent;
    renormalize_tbn( pin.normal, pin.tangent, bitangent );
    
    float3 normal = unpack_normal( normal_map.Sample( linear_wrap_sampler, pin.uv ).xy,
                                   pin.tangent, bitangent, pin.normal );

    float roughness;
    float metallic;
    unpack_specular( specular_map.Sample( linear_wrap_sampler, pin.uv ).rgb, roughness, metallic );
    
    float3 diffuse_albedo = (1.0f - metallic) * base_color.rgb;
    float3 fresnel_r0 = lerp( material.diffuse_fresnel, base_color.rgb, metallic );
    
    float3 to_camera = normalize( -pin.pos_v );
    float cos_nv = saturate( dot( to_camera, normal ) );

    // calc direct
    PixelOut res;

    res.direct_lighting.rgb = make_float3( 0.0f );
    for ( int light_idx = 0; light_idx < pass_params.n_parallel_lights; ++light_idx )
    {
        ParallelLight light = pass_params.parallel_lights[light_idx];

        float3 light_radiance = light.strength * parallel_direct_lighting( diffuse_albedo, fresnel_r0,
                                                                           light.dir, to_camera, normal, cos_nv,
                                                                           roughness );

        res.direct_lighting.rgb +=  light_radiance * csm_shadow_factor( pin.pos_v, light, shadow_cascade,
                                                                        pass_params.csm_split_positions, shadow_map_sampler );
    }

    // calc ambient
    float4 irradiance_dir = mul( float4( normal, 0.0f ), pin.view2env );
    float4 reflection_dir = mul( float4( reflect( pin.pos_v, normal ), 0.0f ), pin.view2env );
    
    res.ambient_lighting.rgb = float3( ibl_radiance_multiplier * diffuse_albedo * irradiance_map.Sample( linear_wrap_sampler, irradiance_dir.xyz ).xyz );
    
    float3 prefiltered_spec_radiance = reflection_probe.SampleLevel( linear_wrap_sampler, reflection_dir.xyz, lerp( 0.1, 1, pow( roughness, 0.5f ) ) * 6.0f ).xyz;
    float2 env_brdf = brdf_lut.Sample( linear_wrap_sampler, float2( cos_nv, roughness ) ).xy;

    res.ambient_lighting.rgb += ibl_radiance_multiplier * prefiltered_spec_radiance
                                * ( fresnel_r0 * env_brdf.x + fresnel_schlick_roughness( 0, fresnel_r0, roughness ) * env_brdf.y );

    // pack data
    res.ambient_lighting.a = 1.0f;
    res.direct_lighting.a = 1.0f;
    res.screen_space_normal = normal.xy;
    return res;
}