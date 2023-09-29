#pragma once

static const float FLT_EPSILON = 1.19209290E-07f;
static const float M_PI = 3.14159265358979323846f;

float2 _float2( float v )
{
    return float2( v, v );
}

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

int DistanceToCursorSqr( int2 test_pos, int2 cursor_pos )
{
    int2 pix_to_cursor = abs( test_pos - cursor_pos );
    return dot( pix_to_cursor, pix_to_cursor );
}

// https://github.com/skeeto/hash-prospector
uint HashUint32( uint x )
{
	x ^= x >> 16;
	x *= 0xa812d533;
	x ^= x >> 15;
	x *= 0xb278e4ad;
	x ^= x >> 17;
	return x;
}

static const float3 COLOR_RED = float3( 1, 0, 0 );
static const float3 COLOR_GREEN = float3( 0, 1, 0 );