#include "lib/lighting.hlsli"

#define PER_OBJECT_CB_BINDING b0
#include "bindings/object_cb.hlsli"

#define PER_PASS_CB_BINDING b1
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
    float2 uv : TEXCOORD;
};

VertexOut main(VertexIn vin)
{
    float4x4 mvp_mat = mul(model_mat, view_proj_mat);
    VertexOut vout;
    vout.pos = mul(float4(vin.pos, 1.0f), mvp_mat);    
    vout.uv = vin.uv;
    return vout;
}