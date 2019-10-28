#include "lib/colorspaces.hlsli"

cbuffer TonemapSettigns : register(b0)
{
    float max_luminance;
    float min_luminance;
    bool blend_luminance;
}

Texture2D frame : register(t0);

float4 main(float4 coord : SV_POSITION) : SV_TARGET
{
    int2 coord2d = int2(coord.x, coord.y);
    float4 cur_radiance = frame.Load(int3(coord2d, 0));
    
    float cur_luminance = photopic_luminance(cur_radiance.rgb);

    float linear_brightness = saturate( (cur_luminance - min_luminance) / (max_luminance - min_luminance) );

    return float4( radiance2gamma_rgb( cur_radiance.rgb, linear_brightness ), cur_radiance.a );
}