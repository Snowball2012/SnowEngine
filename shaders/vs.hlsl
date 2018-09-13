#include "lib/lighting.hlsli"

#define PER_OBJECT_CB_BINDING b0
#include "bindings/object_cb.hlsli"

#define PER_PASS_CB_BINDING b2
#include "bindings/pass_cb.hlsli"

struct VertexIn
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

struct VertexOut
{
	float4 pos : SV_POSITION;
	float3 pos_w : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

VertexOut main( VertexIn vin )
{
	float4x4 mvp_mat = mul( model_mat, view_proj_mat );
	VertexOut vout;
	float4 pos_w = mul( float4( vin.pos, 1.0f ), model_mat );
	vout.pos_w = pos_w.xyz / pos_w.w;
	vout.pos = mul( float4( vin.pos, 1.0f ), mvp_mat );
    
    if ( use_linear_depth > 0 )
        vout.pos.z *= vout.pos.w / far_z; // linearize depth

	vout.normal = normalize( mul( float4( vin.normal, 0.0f ), model_inv_transpose_mat ).xyz );
	vout.uv = vin.uv;
	return vout;
}