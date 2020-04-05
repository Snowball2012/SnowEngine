#include "lib/lighting.hlsli"

#define PER_OBJECT_CB_BINDING b0
#include "bindings/object_cb.hlsli"

cbuffer cbLightInfo : register(b1)
{
    uint light_idx;
    uint split_idx;
}

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
    float4 pos_v : SV_POSITION;
    float2 uv : TEXCOORD;
};

VertexOut main(VertexIn vin)
{
    float4x4 mv_mat = mul( renderitem.model_mat, pass_params.view_mat );
    VertexOut vout;
    
    ParallelLight light = pass_params.parallel_lights[light_idx];
    float4x4 shadow_mat = light.shadow_map_mat[split_idx];
    
    vout.pos_v = mul( mul( float4( vin.pos, 1.0f ), mv_mat ), shadow_mat );    
    
    vout.uv = vin.uv;
    
    return vout;
}