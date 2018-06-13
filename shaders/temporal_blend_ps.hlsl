cbuffer BlendSettings : register( b0 )
{
	float blend_val;
}

SamplerState point_wrap_sampler : register( s0 );

Texture2D prev_frame : register( t0 );

float4 main( float4 coord : SV_POSITION ) : SV_TARGET
{
	int3 texcoord = int3( coord.x, coord.y, 0 );
	return float4( prev_frame.Load( texcoord ).xyz, blend_val );
}