cbuffer cbPerObject : register( b0 )
{
	float4x4 model_mat;
}

cbuffer cbPerPass : register( b1 )
{
	float4x4 view_mat;
	float4x4 view_inv_mat;
	float4x4 proj_mat;
	float4x4 proj_inv_mat;
	float4x4 view_proj_mat;
	float4x4 view_proj_inv_mat;
	float3 eye_pos_w;
	float cb_per_object_pad_1;
	float2 render_target_size;
	float2 render_target_size_inv;
	float near_z;
	float far_z;
	float total_time;
	float delta_time;
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
	float4x4 mvp_mat = mul( model_mat, view_proj_mat );
	VertexOut vout;
	vout.pos = mul( float4( vin.pos, 1.0f ), mvp_mat );
	vout.color = vin.color;
	return vout;
}