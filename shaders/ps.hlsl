#include "lighting_utils.hlsl"

struct PixelIn
{
	float4 pos : SV_POSITION;
	float3 pos_w : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

float3 rendering_equation( float3 to_source, float3 to_camera, float3 normal, float2 uv, float3 light_strength )
{
	float lambert_term = dot( to_source, normal );

	float4 diffuse_albedo = albedo_map.Sample( anisotropic_wrap_sampler, uv );

	return ( lambert_term * light_strength )
		* ( diffuse_albedo.rgb
			+ specular_strength( fresnel_r0,
								 normal,
								 normalize( halfvector( to_source, to_camera ) ),
								 roughness ) );
}

float4 main( PixelIn pin ) : SV_TARGET
{
	float3 res_color = float3( 0.0f, 0.0f, 0.0f );
	//res_color = pin.normal / 2.0f + float3( 0.5f, 0.5f, 0.5f );
	for ( int light_idx = 0; light_idx < n_parallel_lights; ++light_idx )
		res_color += rendering_equation( lights[light_idx].dir,
										 normalize( eye_pos_w - pin.pos_w ),
										 normalize( pin.normal ),
										 pin.uv,
										 lights[light_idx].strength );

	return float4( res_color, 1.0f );
}