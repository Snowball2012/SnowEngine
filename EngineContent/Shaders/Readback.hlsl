#include "SceneViewParams.hlsli"

[[vk::binding( 0 )]] RWStructuredBuffer<ViewFrameReadbackData> readback_data;

[numthreads(1, 1, 1)]
void ReadbackDataClearCS( uint3 dispatch_thread_id : SV_DispatchThreadID )
{
    ViewFrameReadbackData data;
    data.picking_id_under_cursor = -1;
    readback_data[0] = data;
}