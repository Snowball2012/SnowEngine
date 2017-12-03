cbuffer cbPerObject : register( b0 )
{
	float4x4 mvp_mat;
}

struct VertexIn
{
	float3 pos : POSITION;
	float4 color : COLOR;
};

struct VertexOut
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
};

VertexOut main( VertexIn vin )
{
	VertexOut vout;
	vout.pos = mul( float4( vin.pos, 1.0f ), mvp_mat );
	vout.color = vin.color;
	return vout;
}