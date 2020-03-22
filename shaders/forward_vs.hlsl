#include "lib/lighting.hlsli"

#define PER_OBJECT_CB_BINDING b0
#include "bindings/object_cb.hlsli"

#define PER_PASS_CB_BINDING b2
#include "bindings/pass_cb.hlsli"

struct IBLTransform
{
    float4x4 world2env_mat;
};

cbuffer cbIBLTransform : register(b3)
{
    IBLTransform ibl;
}

struct VertexIn
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

struct VertexOut
{
    nointerpolation float4x4 view2env : VIEWTOENV;
    float4 pos : SV_POSITION;
    float3 pos_v : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

VertexOut main( VertexIn vin )
{
    float4 pos_ws = mul( float4( vin.pos, 1.0f ), renderitem.model_mat );
    VertexOut vout;
    float4 pos_v = mul( pos_ws, pass_params.view_mat );
    vout.pos = mul( pos_ws, pass_params.view_proj_mat );
    vout.pos_v = pos_v.xyz / pos_v.w;

    vout.normal = normalize( mul( mul( float4( vin.normal, 0.0f ), renderitem.model_inv_transpose_mat ), pass_params.view_mat ).xyz );
    vout.tangent = normalize( mul( mul( float4( vin.tangent, 0.0f ), renderitem.model_inv_transpose_mat ), pass_params.view_mat ).xyz );
    vout.uv = vin.uv;
    vout.view2env = mul( pass_params.view_inv_mat, ibl.world2env_mat );
    return vout;
}