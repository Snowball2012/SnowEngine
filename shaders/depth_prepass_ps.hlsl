
#define ALPHA_BIAS 0.1f

struct PixelIn
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D base_color_map : register( t0 );
SamplerState base_color_sampler : register( s0 );

void main( PixelIn pin )
{
    clip( base_color_map.Sample( base_color_sampler, pin.uv).a - ALPHA_BIAS );
	return;
}