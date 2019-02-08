cbuffer FaceCB : register( b0 )
{
    float4x4 viewproj_inverse;
}

// parabolic hdr skybox
Texture2D src : register( t0 );

SamplerState linear_wrap_sampler : register(s0);

static const float M_PI = 3.14159265f;

struct PixelIn
{
    float4 pos : SV_POSITION;
    float4 pos_ndc : NDCCOORD;
};

float4 main( PixelIn pin ) : SV_TARGET
{
    float3 ray_ws = normalize( mul( pin.pos_ndc, viewproj_inverse ).xyz );
    float2 uv;
    uv.y = 0.5f - asin( ray_ws.y ) / M_PI;
    uv.x = ( atan2( ray_ws.x, ray_ws.z ) + M_PI ) / (2 * M_PI);

    return float4( src.Sample( linear_wrap_sampler, uv ).rgb, 1.0f );
}