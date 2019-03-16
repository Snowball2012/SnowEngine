#include "lib/lighting.hlsli"

#define PER_OBJECT_CB_BINDING b0
#include "bindings/object_cb.hlsli"

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

struct VertexIn
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

struct VertexOut
{
    nointerpolation float4x4 view2env : VIEWTOENV;
	float4 pos : SV_POSITION;
	float3 pos_v : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

VertexOut main( VertexIn vin )
{
	float4x4 mvp_mat = mul( renderitem.model_mat, pass_params.view_proj_mat );
	VertexOut vout;
	float4 pos_v = mul( mul( float4( vin.pos, 1.0f ), renderitem.model_mat ), pass_params.view_mat );
	vout.pos_v = pos_v.xyz / pos_v.w;
	vout.pos = mul( float4( vin.pos, 1.0f ), mvp_mat );

	vout.normal = normalize( mul( mul( float4( vin.normal, 0.0f ), renderitem.model_inv_transpose_mat ), pass_params.view_mat ).xyz );
	vout.uv = vin.uv;
    vout.view2env = mul( pass_params.view_inv_mat, ibl.world2env_mat );
	return vout;
}