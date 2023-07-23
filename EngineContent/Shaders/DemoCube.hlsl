
struct VertexIn
{
	[[vk::location( 0 )]] float3 position : POSITION;
};

struct Matrices
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4x4 view_proj_inv;
    uint2 viewport_size;
};

[[vk::binding( 0 )]] ConstantBuffer<Matrices> obj_data : register( b0 );

[[vk::location( 0 )]] float4 TriangleVS( in VertexIn vertex_data, [[vk::location( 1 )]] out float3 color : COLOR ) : SV_POSITION
{
	color = vertex_data.position + float3( 0.5f, 0.5f, 0.5f );
	float4 pos_cs = mul( obj_data.proj, mul( obj_data.view, mul( obj_data.model, float4( vertex_data.position, 1.0f ) ) ) );
	return pos_cs;
}

[[vk::location( 0 )]] float4 TrianglePS( [[vk::location( 1 )]] in float3 color : COLOR ) : SV_TARGET0
{
	return float4( color, 1.0f );
}