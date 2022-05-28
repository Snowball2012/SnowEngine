
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
    float2 plane_ndc = (float2(pixel_id) / (pass_params.render_target_size - 1.0f)) * 2.0f - 1.0f;
    plane_ndc.y *= -1;
    float4 ndc = float4( plane_ndc, depth_buffer.Load( uint3(pixel_id, 0) ).r, 1.0f );
    
    float4 position_ws = mul(ndc, pass_params.view_proj_inv_mat);
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

    float2 plane_ndc = float2(pixel_id) / (pass_params.render_target_size - 1.0f) * 2.0f - 1.0f;

    float closeness_to_edge = dot(plane_ndc, plane_ndc);
    float ray_tolerance = lerp(1.e-2f, 3.e-2f, closeness_to_edge);
    
    float3 base_dir = mul(pass_params.view_mat, pass_params.parallel_lights[0].dir);

    float2 seed = frac(plane_ndc + pass_params.total_time);

    int nsamples = 1;
    float visibility_acc = 0;
    float seed_step = 1.0f / float(nsamples);

    for (int i = 0; i < nsamples; ++i)
    {
        float2 random_val = random_from_seed(seed);
        seed = frac(seed + seed_step);

        float3 cone_sample = uniform_sample_cone(random_val, pass_params.parallel_lights[0].half_angle_sin_2).xyz;
        float3 ray_dir = apply_cone_sample_to_direction(cone_sample, base_dir);

        visibility_acc += ShadowRay(ray_origin, ray_dir.xyz, ray_tolerance);
    }

    output[pixel_id] = visibility_acc / nsamples;
}