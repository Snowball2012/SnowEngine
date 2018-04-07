#include "lighting_utils.hlsl"

struct PixelIn
{
	float4 pos : SV_POSITION;
	float3 pos_w : POSITION;
	float3 normal : NORMAL;
};

float3 rendering_equation( float3 to_source, float3 to_camera, float3 normal, float3 light_strength )
{
	float lambert_term = dot( to_source, normal );

	return ( lambert_term * light_strength )
		* ( diffuse_albedo.rgb
			+ specular_strength( fresnel_r0,
								 normalize( normal ),
								 normalize( halfvector( to_source, to_camera ) ),
								 roughness ) );
}

float4 main( PixelIn pin ) : SV_TARGET
{
	float3 res_color = float3( 0.0f, 0.0f, 0.0f );
	for ( int light_idx = 0; light_idx < n_parallel_lights; ++light_idx )
		res_color += rendering_equation( lights[light_idx].dir,
										 normalize( eye_pos_w - pin.pos_w ),
										 pin.normal,
										 lights[light_idx].strength );

	return float4( res_color, 1.0f );
}