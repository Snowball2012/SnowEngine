#include "Utils.hlsli"

[[vk::binding( 0 )]]
Texture2D<float4> TextureObject;
[[vk::binding( 1 )]]
SamplerState TextureObject_Sampler;

struct DisplayMappingParams
{
    uint method;
	float white_point;
};
[[vk::binding( 2 )]] ConstantBuffer<DisplayMappingParams> pass_params;

static const uint DISPLAY_MAPPING_METHOD_LINEAR = 0;
static const uint DISPLAY_MAPPING_METHOD_REINHARD = 1;
static const uint DISPLAY_MAPPING_METHOD_REINHARD_WHITE_POINT = 2;

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
	float3 input_linear = TextureObject.Sample( TextureObject_Sampler, uv ).rgb;
	
    float3 output = float3( 0, 1, 0 );
	
	if ( pass_params.method == DISPLAY_MAPPING_METHOD_LINEAR )
    {
        output = input_linear / pass_params.white_point;
    }
	else if ( pass_params.method == DISPLAY_MAPPING_METHOD_REINHARD )
    {
		output = input_linear / ( _float3( 1.0f ) + input_linear );
    }
	else if ( pass_params.method == DISPLAY_MAPPING_METHOD_REINHARD_WHITE_POINT )
    {
		output = input_linear * ( _float3( 1.0f ) + input_linear / sqr( pass_params.white_point ) ) / ( _float3( 1.0f ) + input_linear );
    }

    return float4( output, 1.0f );
}