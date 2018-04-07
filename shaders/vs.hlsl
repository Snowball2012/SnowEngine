#include "lighting_utils.hlsl"

struct VertexIn
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
};

struct VertexOut
{
	float4 pos : SV_POSITION;
	float3 pos_w : POSITION;
	float3 normal : NORMAL;
};

VertexOut main( VertexIn vin )
{
	float4x4 mvp_mat = mul( model_mat, view_proj_mat );
	VertexOut vout;
	vout.pos_w = vin.pos;
	vout.pos = mul( float4( vin.pos, 1.0f ), mvp_mat );
	vout.normal = mul( float4( vin.normal, 0.0f ), model_mat ).xyz;
	return vout;
}