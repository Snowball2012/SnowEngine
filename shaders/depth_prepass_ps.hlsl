
#define ALPHA_BIAS 0.001f

struct PixelIn
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D base_color_map : register( t0 );
SamplerState base_color_sampler : register( s0 );

float4 main( PixelIn pin ) : SV_TARGET
{
    clip( base_color_map.Sample( base_color_sampler, pin.uv).a - ALPHA_BIAS );
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}