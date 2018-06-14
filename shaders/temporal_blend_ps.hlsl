cbuffer BlendSettings : register( b0 )
{
	float blend_val;
	float jitter_x;
	float jitter_y;
}

SamplerState point_wrap_sampler : register( s0 );

Texture2D prev_frame : register( t0 );
Texture2D cur_frame : register( t1 );

float4 main( float4 coord : SV_POSITION ) : SV_TARGET
{
	float2 tex_size;
	prev_frame.GetDimensions( tex_size.x, tex_size.y );
	float2 texcoord = float2( coord.x + jitter_x, coord.y + jitter_y ) / tex_size;

	float4 cur_color = cur_frame.Sample( point_wrap_sampler, texcoord );
	float4 hist_color = prev_frame.Load( int3( coord.x, coord.y, 0 ) );

	// 3x3 neighbourhood clamping 
	const float2 offsets[8] =
	{
		float2( -1, -1 ), float2( 0, -1 ), float2( 1, -1 ),
		float2( -1, 0 ), float2( 1, 0 ),
		float2( -1, 1 ), float2( 0, 1 ), float2( 1, 1 ),
	};

	float4 min_color = cur_color;
	float4 max_color = cur_color;


	[unroll]
	for ( int i = 0; i < 8; ++i )
	{
		float4 color = cur_frame.Sample( point_wrap_sampler, texcoord + offsets[i] / tex_size );
		min_color = min( color, min_color );
		max_color = max( color, max_color );
	}
	hist_color = clamp( hist_color, min_color, max_color );


	return lerp( float4( cur_color.xyz, 1 ), float4( hist_color.xyz, 1 ), blend_val );
}