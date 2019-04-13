#ifndef LIGHT_CB_HLSLI
#define LIGHT_CB_HLSLI

struct Light
{
    float4x4 shadow_map_mat;
    float3 strength;
    float falloff_start;
    float3 origin;
    float falloff_end;
    float3 dir;
    float spot_power;
};

#define MAX_CASCADE_SIZE 4
struct ParallelLight
{
    float4x4 shadow_map_mat[MAX_CASCADE_SIZE];
    float3 strength;
    int csm_num_split_positions;
    float3 dir;
    float _padding;
};

#endif