
struct VertexIn
{
	[[vk::location(0)]] float2 position : POSITION;
	[[vk::location(1)]] float3 color : TEXCOORD0;
	[[vk::location(2)]] float2 texcoord : TEXCOORD1;
};

struct Matrices
{
	float4x4 model;
	float4x4 view;
	float4x4 proj;
};

[[vk::binding(0)]] ConstantBuffer<Matrices> obj_data : register(b0);

[[vk::binding(1)]]
Texture2D<float4> TextureObject;
[[vk::binding(2)]]
SamplerState TextureObject_Sampler;


[[vk::location(0)]] float4 TriangleVS(in VertexIn vertex_data, [[vk::location(1)]] out float3 color : COLOR, [[vk::location(2)]] out float2 texcoord : TEXCOORD1) : SV_POSITION
{
	color = vertex_data.color;
	float4 pos_cs = mul(obj_data.proj, mul(obj_data.view, mul(obj_data.model, float4(vertex_data.position, 0.0f, 1.0f))));
	texcoord = vertex_data.texcoord;
	return pos_cs;
}

[[vk::location(0)]] float4 TrianglePS([[vk::location(1)]] in float3 color : COLOR, [[vk::location(2)]] in float2 texcoord : TEXCOORD1 ) : SV_TARGET0
{

	float4 TexSample = TextureObject.Sample(TextureObject_Sampler, texcoord);

	return float4(TexSample.rgb * color, 1.0f);
}