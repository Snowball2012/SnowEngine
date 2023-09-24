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