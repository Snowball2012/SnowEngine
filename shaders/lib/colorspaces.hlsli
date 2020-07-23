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

float photopic_luminance( float3 radiance )
{
    // rgb radiance in watt/(sr*m^2)
    return dot( 683.0f * float3( 0.2973f, 1.0f, 0.1010f ), radiance.rgb );
}

float3 calc_white_point( float luminance )
{
    return float3(1,1,1) * luminance / 955.0389f;
}

float percieved_brightness( float3 color )
{
    return (0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b);
}

float3 gamma_2_2_OETF( float3 color )
{
    return pow( color, 1.0f/2.2f );
}

float3 srgb_OETF( float3 color )
{
    float3 linear_part = 12.92f * color;
    float3 gamma_part = 1.055f * pow( color, 1.0f/2.4f ) - 0.055f;
    return lerp( linear_part, gamma_part, color > ((float3)0.0031308f) );
}

#endif