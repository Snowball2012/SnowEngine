#include "lighting_utils.hlsl"

struct PixelIn
{
	float4 pos : SV_POSITION;
	float3 pos_w : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
	float2 uv : TEXCOORD;
};

float3 rendering_equation( float3 to_source, float3 to_camera, float3 normal, float2 uv, float3 light_strength )
{
	float lambert_term = max( dot( to_source, normal ), 0 );
	float normal_to_eye_cos = max( dot( to_camera, normal ), 0 );
	float3 h = normalize( halfvector( to_source, to_camera ) );
	float source_to_half_cos = max( dot( to_source, h ), 0 );

	float4 base_color = base_color_map.Sample( anisotropic_wrap_sampler, uv );

	// alpha test
	clip( base_color.a - 0.01 );

	float3 specular = specular_map.Sample( anisotropic_wrap_sampler, uv ).rgb; // r - occlusion, g - roughness, b - metallic
	float roughness = specular.g;
	float metallic = specular.b;

	float3 diffuse_albedo = (1.0f - metallic) * base_color.rgb;
	float3 fresnel_r0 = lerp( diffuse_fresnel, base_color.rgb, metallic );

	return ( lambert_term * light_strength )
		    * ( diffuse_disney( roughness, lambert_term, normal_to_eye_cos, source_to_half_cos ) * diffuse_albedo 
				+ specular_strength( fresnel_r0,
									 dot( normal, h ),
									 source_to_half_cos,
									 h,
									 roughness ) );
}

float3 ws_normal_bump_mapping( float3 ws_normal, float3 ws_tangent, float3 ws_binormal, float2 ts_normal_compressed )
{
	ts_normal_compressed = 2 * ts_normal_compressed - float2( 1.0f, 1.0f );
	float tangent_normal_z = sqrt( 1.0 - pow( ts_normal_compressed.x, 2 ) - pow( ts_normal_compressed.y, 2 ) );
	return normalize( ts_normal_compressed.x * ws_tangent + ts_normal_compressed.y * ws_binormal + tangent_normal_z * ws_normal );
}

float4 main( PixelIn pin ) : SV_TARGET
{
	float3 res_color = float3( 0.0f, 0.0f, 0.0f );

	float3 normal = ws_normal_bump_mapping( normalize( pin.normal ),
											normalize( pin.tangent ),
											normalize( pin.binormal ),
											normal_map.Sample( linear_wrap_sampler, pin.uv ).xy );

	for ( int light_idx = 0; light_idx < n_parallel_lights; ++light_idx )
	{
		float3 light_intensity = rendering_equation( lights[light_idx].dir,
												 normalize( eye_pos_w - pin.pos_w ),
												 normal,
												 pin.uv,
												 lights[light_idx].strength );


		res_color += light_intensity * max( shadow_factor( pin.pos_w, lights[light_idx].shadow_map_mat ), 0.1f );
	}

	//return float4( 0.5 * ( normal + float3( 1.0f, 1.0f, 1.0f ) ), 1.0f ); // normal
	return float4( res_color, 1.0f );
}