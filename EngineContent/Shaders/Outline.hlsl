#include "Utils.hlsli"

[[vk::binding( 0 )]] RWTexture2D<uint> object_id_tex;
[[vk::binding( 1 )]] RWTexture2D<float4> output_tex;

struct OutlineParams
{
    uint object_id;
};
[[vk::binding( 2 )]] ConstantBuffer<OutlineParams> pass_params;

[numthreads(8, 8, 1)]
void ComputeOutlineCS( uint3 dispatch_thread_id : SV_DispatchThreadID )
{
    uint pixel_obj_id = object_id_tex[ dispatch_thread_id.xy ];
    if ( pixel_obj_id == pass_params.object_id )
    {
        return;
    }
    
    int2 cur_pix_location = dispatch_thread_id.xy;
    bool is_outline_pixel = false;
    for ( int j = -1; j <= 1; ++j )
    {
        for ( int i = -1; i <= 1; ++i )
        {
            int2 sample_location = cur_pix_location + int2( i, j );
            uint sample_obj_id = object_id_tex[ sample_location ];
            if ( sample_obj_id == pass_params.object_id )
            {
                is_outline_pixel = true;
            }
        }
    }
     
    if ( is_outline_pixel )
    {
        output_tex[ cur_pix_location ] = float4( 1, 1, 0, 1 );
    }
}