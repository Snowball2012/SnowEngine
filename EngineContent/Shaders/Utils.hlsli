#pragma once

static const float FLT_EPSILON = 1.19209290E-07f;

float3 _float3( float v )
{
    return float3( v, v, v );
}

float sqr( float v )
{
    return v * v;
}

// We deliberately avoid the term "Luminance" here since this value is not liked to any physical units of measurement
float CalcLinearBrightness( float3 v )
{
    return dot( v, float3( 0.2126f, 0.7152f, 0.0722f ) );
}

float3 ColorFromIndex( uint idx )
{
    return float3( ( idx % 3 ) == 0, ( idx % 3 ) == 1, ( idx % 3 ) == 2 );
}
