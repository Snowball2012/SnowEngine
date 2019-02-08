#define PER_PASS_CB_BINDING b0
#include "bindings/pass_cb.hlsli"

cbuffer SkyboxParamsCB : register( b1 )
{
    float radiance_multiplier;
}

// parabolic hdr skybox
Texture2D skybox : register( t0 );

SamplerState linear_wrap_sampler : register(s0);

struct PixelIn
{
    float4 pos : SV_POSITION;
    float4 pos_ndc : NDCCOORD;
};

float4 main( PixelIn pin ) : SV_TARGET
{
    float4 pos = pin.pos_ndc;
    // reconstruct ray vector in world space
    pos.xyz /= pos.w;
    pos.w = 1.0f;
    pos.z = 1.0f;
    float4 ray_ws = mul( pos, pass_params.proj_inv_mat );

    ray_ws.w = 0;
    ray_ws.xyz =normalize( mul( ray_ws, pass_params.view_inv_mat ).xyz );
    //ray_ws.xyz = normalize( mul( ray_ws, pass_params.view_inv_mat ).xyz );
    float2 uv;
    uv.y = 0.5f - asin( ray_ws.y ) / M_PI;
    uv.x = ( atan2( ray_ws.x, ray_ws.z ) + M_PI ) / (2 * M_PI);

    return float4( skybox.Sample( linear_wrap_sampler, uv ).rgb * radiance_multiplier, 1.0f );
}