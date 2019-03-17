#ifndef COLORSPACES_HLSLI
#define COLORSPACES_HLSLI

#include "math_utils.hlsli"

static const float STANDART_GAMMA = 2.2f;


float gamma2linear( float val, float gamma )
{
    return pow( val, gamma );
}

float4 gamma2linear( float4 val, float gamma )
{
    return pow( val, gamma );
}

float3 linear2gamma( float3 val, float gamma )
{
    return pow( val, 1.0f / gamma );
}


float gamma2linear_std( float val )
{
    return gamma2linear( val, STANDART_GAMMA );
}

float4 gamma2linear_std( float4 val )
{
    return gamma2linear( val, STANDART_GAMMA );
}

float3 linear2gamma_std( float3 val, float gamma )
{
    return linear2gamma( val, STANDART_GAMMA );
}


float photopic_luminance(float3 radiance)
{
    // rgb radiance in watt/(sr*m^2)
    return 683.0f * (0.2973f * radiance.r + 1.0f * radiance.g + 0.1010f * radiance.b);
}


float percieved_brightness(float3 color)
{
    return (0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b);
}


float3 radiance2gamma_rgb(float3 radiance, float brightness)
{
    float3 normalized_color = radiance / percieved_brightness(radiance);
    float3 linear_color = normalized_color * brightness;
    return pow(linear_color, 1.0f / STANDART_GAMMA );
}

#endif