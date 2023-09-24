#include "SceneViewParams.hlsli"

[[vk::binding( 0 )]] RWStructuredBuffer<DebugLineIndirectArgs> indirect_args;

[numthreads(1, 1, 1)]
void DebugDrawingInitCS( uint3 dispatch_thread_id : SV_DispatchThreadID )
{
    DebugLineIndirectArgs init_args;
    init_args.vertex_per_instance = 2;
    init_args.num_instances = 0;
    init_args.start_vertex_location = 0;
    init_args.start_instance_location = 0;
    indirect_args[0] = init_args;
}

[[vk::location( 0 )]] float4 DebugDrawingDrawLinesVS( in uint vertex_id : SV_VertexID, in uint instance_id : SV_InstanceID, [[vk::location( 1 )]] out float3 color : COLOR ) : SV_POSITION
{
    uint line_idx = instance_id;
    
    DebugLine line_to_draw = debug_lines[line_idx];
    
    float3 pos_ws = vertex_id % 2 == 0 ? line_to_draw.start_ws : line_to_draw.end_ws;
    color = vertex_id % 2 == 0 ? line_to_draw.color_start : line_to_draw.color_end;
    
    float4 pos_clip = mul( view_data.proj_mat, mul( view_data.view_mat, float4( pos_ws, 1.0f ) ) );
    
    return pos_clip;
}

[[vk::location( 0 )]] float4 DebugDrawingDrawLinesPS( [[vk::location( 1 )]] in float3 color : COLOR ) : SV_TARGET0
{
	return float4( color, 1.0f );
}