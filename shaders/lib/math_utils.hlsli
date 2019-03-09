#ifndef MATH_UTILS_HLSLI
#define MATH_UTILS_HLSLI

float sqr( float a )
{
	return a * a;
}

float2 make_float2( float val )
{
    return float2( val, val );    
}

float3 make_float3( float val )
{
    return float3( val, val, val );
}

float4 make_float4( float val )
{
    return float4( val, val, val, val );
}

static const float PI = 3.14159265f;

#endif