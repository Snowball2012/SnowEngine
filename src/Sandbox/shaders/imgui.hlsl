
struct VertexIn
{
	[[vk::location( 0 )]] float2 position : POSITION;
	[[vk::location( 1 )]] float2 uv : TEXCOORD0;
	[[vk::location( 2 )]] float4 color : TEXCOORD1;
};


struct Interpolants
{
	[[vk::location( 0 )]] float4 sv_position : SV_POSITION;
	[[vk::location( 1 )]] float2 uv : TEXCOORD0;
	[[vk::location( 2 )]] float4 color : TEXCOORD1;
};

struct PixelOut
{
	[[vk::location( 0 )]] float4 rt0 : SV_TARGET0;
};

[[vk::binding( 0 )]]
Texture2D<float4> TextureObject;
[[vk::binding( 1 )]]
SamplerState TextureObject_Sampler;

struct PushConstants {
	float2 scale;
	float2 translate;
};

[[vk::push_constant]]
PushConstants pc;

void MainVS( in VertexIn vin, out Interpolants vout )
{
	vout.uv = vin.uv;
	vout.color = vin.color;
	vout.sv_position = float4( vin.position * pc.scale + pc.translate, 0, 1 );
}

void MainPS( in Interpolants pin, out PixelOut pout )
{
	pout.rt0 = pin.color * TextureObject.Sample( TextureObject_Sampler, pin.uv );
	pout.rt0.rgb *= pout.rt0.a;
}
