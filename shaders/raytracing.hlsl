
#include "lib/math_utils.hlsli"

#define PER_PASS_CB_BINDING b0
#include "bindings/pass_cb.hlsli"


Texture2D<float> depth_buffer : register(t0);

RaytracingAccelerationStructure scene_tlas : register(t1);

RWTexture2D<float4> output : register(u0);

struct ShadowPayload
{
    float is_visible;
};


float4 reconstruct_position_ws(uint2 pixel_id)
{
    float2 plane_ndc = float2(pixel_id) / (pass_params.render_target_size - 1.0f) * 2.0f - 1.0f;
    float4 ndc = float4( plane_ndc, depth_buffer.Load( uint3(pixel_id, 0) ).r, 1.0f );
    
    float4 position_ws = ndc * pass_params.view_proj_inv_mat;
    position_ws /= position_ws.w;
    return position_ws;
}

float ShadowRay(float3 o, float3 d, float min_t, float max_t = FLT_MAX)
{
    RayDesc ray = {o, min_t, d, max_t};

    ShadowPayload pay = { 0 };
    
    TraceRay(scene_tlas,
        (RAY_FLAG_SKIP_CLOSEST_HIT_SHADER |
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH),
        0xFF, 0, 1, 0, ray, pay);

    return pay.is_visible; 
}

[shader("miss")]
void DirectShadowmaskMS(inout ShadowPayload payload)
{
    payload.is_visible = 1;
}

[shader("anyhit")]
void DirectShadowmaskAHS(inout ShadowPayload pay, BuiltInTriangleIntersectionAttributes attrib)
{
}

[shader("raygeneration")]
void DirectShadowmaskRGS()
{
    uint2 pixel_id = DispatchRaysIndex().xy;

    float3 ray_origin = reconstruct_position_ws(pixel_id);
    float3 ray_dir = pass_params.lights[0].dir;
    
    output[pixel_id] = ShadowRay(ray_origin, ray_dir, 1.e-4f);
    
}