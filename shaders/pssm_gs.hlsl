#include "lib/lighting.hlsli"

cbuffer cbLightInfo : register( b1 )
{
    uint light_idx;
}

#define PER_PASS_CB_BINDING b2
#include "bindings/pass_cb.hlsli"

struct GSInput
{
    float4 pos_v : SV_POSITION;
    float2 uv : TEXCOORD;
};

struct GSOutput
{
	float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    uint split_idx : SV_RenderTargetArrayIndex;
};


[maxvertexcount(3 * MAX_CASCADE_SIZE)]
void main( triangle GSInput input[3], 
           inout TriangleStream<GSOutput> output )
{
    ParallelLight light = pass_params.parallel_lights[light_idx];
    for ( uint split_idx = 0; split_idx < light.csm_num_split_positions + 1; ++split_idx )
    {
        float4x4 shadow_mat = light.shadow_map_mat[split_idx];
		output.RestartStrip();
	    for (uint i = 0; i < 3; i++)
	    {
		    GSOutput element;
            element.pos = mul( input[i].pos_v, shadow_mat ); 
            element.uv = input[i].uv;
            element.split_idx = split_idx;
		    output.Append(element);
	    }
    }
}