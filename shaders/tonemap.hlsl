cbuffer TonemapSettigns : register(b0)
{
    float max_luminance;
    float min_luminance;
    bool blend_luminance;
}

Texture2D frame : register(t0);

float PhotopicLuminance(float3 radiance)
{
    // rgb radiance in watt/(sr*m^2), r ~ 610nm, g ~ 555nm, b ~ 465nm
    return 683.0f * (0.5f * radiance.r + 1.0f * radiance.g + 0.07f * radiance.b);
}

float PercievedBrightness( float3 color )
{
    return (0.5f * color.r + 1.0f * color.g + 0.07f * color.b) / 1.57f;
}

float3 Radiance2LinearRGB(float3 radiance, float brightness)
{
    float3 normalized_color = radiance / PercievedBrightness(radiance);
    return normalized_color * brightness;
}

float4 main(float4 coord : SV_POSITION) : SV_TARGET
{
    int2 coord2d = int2(coord.x, coord.y);
    float4 cur_radiance = frame.Load(int3(coord2d, 0));
    
    float cur_luminance = PhotopicLuminance(cur_radiance.rgb);
 
    if (blend_luminance)
    {
        const int2 offsets[8] =
        {
            int2(-1, -1), int2(0, -1), int2(1, -1),
		    int2(-1, 0), int2(1, 0),
		    int2(-1, 1), int2(0, 1), int2(1, 1),
        };
        [unroll]
        for (int i = 0; i < 8; ++i)
        {
            float l = PhotopicLuminance(frame.Load(int3(coord2d + offsets[i], 0)).rgb);
            cur_luminance += l;
        }
        cur_luminance /= 9;
    }

    float linear_brightness = clamp((cur_luminance - min_luminance) / (max_luminance - min_luminance), 0, 1);



    return float4(Radiance2LinearRGB(cur_radiance.rgb, pow(linear_brightness, 1.0f / 2.2f)), cur_radiance.a);
}