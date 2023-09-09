[[vk::binding( 0 )]]
Texture2D<float4> TextureObject;
[[vk::binding( 1 )]]
SamplerState TextureObject_Sampler;

static const float2 full_screen_triangle_uv[3] =
{
	float2( 0, 0 ),
	float2( 0, 2 ),
	float2( 2, 0 )
};

static const float2 full_screen_triangle_ndc[3] =
{
	float2( -1, 1 ),
	float2( -1, -3 ),
	float2( 3, 1 )
};

[[vk::location( 0 )]] float4 DisplayMappingVS( in uint vertex_id : SV_VertexID, [[vk::location( 1 )]] out float2 uv : TEXCOORD0 ) : SV_POSITION
{
	float4 pos = float4( full_screen_triangle_ndc[vertex_id], 0.5f, 1 );
	uv = full_screen_triangle_uv[vertex_id];
	return pos;
}

[[vk::location( 0 )]] float4 DisplayMappingPS( [[vk::location( 1 )]] in float2 uv : TEXCOORD0 ) : SV_TARGET0
{
	float4 TexSample = TextureObject.Sample( TextureObject_Sampler, uv );

	return TexSample;
}