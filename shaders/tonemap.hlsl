#include "lib/colorspaces.hlsli"

cbuffer TonemapSettigns : register(b0)
{
    float max_luminance;
    float min_luminance;
    bool blend_luminance;
}

Texture2D frame : register(t0);

Texture2D ambient : register(t1);
Texture2D ssao : register(t2);

SamplerState linear_wrap_sampler : register( s0 );

//#define DEBUG_SSAO

float4 main(float4 coord : SV_POSITION) : SV_TARGET
{
    int2 coord2d = int2(coord.x, coord.y);
    float4 cur_radiance = frame.Load(int3(coord2d, 0));

    float2 rt_dimensions;
    frame.GetDimensions( rt_dimensions.x, rt_dimensions.y );

    float4 ambient_radiance = ambient.Load(int3(coord2d, 0)) * ssao.Sample( linear_wrap_sampler, coord.xy / rt_dimensions ).r;
    cur_radiance += ambient_radiance;
    
    float cur_luminance = photopic_luminance(cur_radiance.rgb);

    float linear_brightness = saturate( (cur_luminance - min_luminance) / (max_luminance - min_luminance) );

    #ifdef DEBUG_SSAO
        float ssao_factor = ssao.Sample( linear_wrap_sampler, coord.xy / rt_dimensions ).r;
        return float4( ssao_factor, ssao_factor, ssao_factor, 1.0f );
    #else
        return float4( radiance2gamma_rgb( cur_radiance.rgb, linear_brightness ), cur_radiance.a );
    #endif
}