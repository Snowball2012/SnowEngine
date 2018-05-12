struct GSInput
{
	float4 pos : SV_POSITION;
	float3 pos_w : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

struct GSOutput
{
	float4 pos : SV_POSITION;
	float3 pos_w : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
	float2 uv : TEXCOORD;
};

[maxvertexcount(3)]
void main(
	triangle GSInput input[3],
	inout TriangleStream< GSOutput > output
)
{
	float3 dp1 = input[1].pos_w - input[0].pos_w;
	float3 dp2 = input[2].pos_w - input[0].pos_w;

	float2 duv1 = input[1].uv - input[0].uv;
	float2 duv2 = input[2].uv - input[0].uv;

	float inv_det = 1.0f / ( duv1.x * duv2.y - duv1.y * duv2.x );
	float3 tangent = normalize( inv_det * ( dp1 * duv2.y - dp2 * duv1.y ) );
	float3 binormal = normalize( inv_det * ( dp2 * duv1.x - dp1 * duv2.x ) );

	for ( uint i = 0; i < 3; i++ )
	{
		GSOutput element;
		element.pos = input[i].pos;
		element.pos_w = input[i].pos_w;
		element.normal = input[i].normal;
		element.uv = input[i].uv;
		element.tangent = tangent;
		element.binormal = binormal;
		output.Append( element );
	}
}