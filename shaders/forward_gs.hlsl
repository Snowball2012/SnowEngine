struct GSInput
{
    float4x4 view2env : VIEWTOENV;
    float4 pos : SV_POSITION;
    float3 pos_v : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct GSOutput
{
    nointerpolation float4x4 view2env : VIEWTOENV;
    float4 pos : SV_POSITION;
    float3 pos_v : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

[maxvertexcount(3)]
void main(
    triangle GSInput input[3],
    inout TriangleStream< GSOutput > output
)
{
    float3 dp1 = input[1].pos_v - input[0].pos_v;
    float3 dp2 = input[2].pos_v - input[0].pos_v;

    float2 duv1 = input[1].uv - input[0].uv;
    float2 duv2 = input[2].uv - input[0].uv;

    float inv_det = 1.0f / ( duv1.x * duv2.y - duv1.y * duv2.x );
    float3 tangent = normalize( inv_det * ( dp1 * duv2.y - dp2 * duv1.y ) );

    for ( uint i = 0; i < 3; i++ )
    {
        GSOutput element;
        element.pos = input[i].pos;
        element.pos_v = input[i].pos_v;
        element.normal = input[i].normal;
        element.uv = input[i].uv;
        element.view2env = input[i].view2env;
        element.tangent = tangent;
        output.Append( element );
    }
}