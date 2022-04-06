#ifndef MATH_UTILS_HLSLI
#define MATH_UTILS_HLSLI


static const float PI = 3.14159265f;

static const float FLT_MAX = 3.402823466e+38f;

static const float GAUSS_KERNEL_3X3_SIGMA1[9] =
{
    0.077847f,	0.123317f,	0.077847f,
    0.123317f,	0.195346f,	0.123317f,
    0.077847f,	0.123317f,	0.077847f,
};

static const float2 OFFSETS_3X3[9] =
{
    float2( -1.0f, -1.0f ), float2( 0.0f, -1.0f ), float2( 1.0f, -1.0f ),
    float2( -1.0f,  0.0f ), float2( 0.0f,  0.0f ), float2( 1.0f,  0.0f ),
    float2( -1.0f,  1.0f ), float2( 0.0f,  1.0f ), float2( 1.0f,  1.0f )
};


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

float2 tc_from_ndc(float2 ndc)
{
    return ((ndc * 0.5f) + (float2)0.5f) * float2(1, -1) + float2(0,1);
}

void renormalize_tbn( inout float3 n, inout float3 t, out float3 b )
{
    // Gram–Schmidt process
    n = normalize( n );
    t = normalize( t - dot( t, n ) * n );
    b = cross( n, t );    
}

float linear_depth( float hyperbolic_z, float near_z, float far_z )
{
    float remapped_z = 2.0 * hyperbolic_z - 1.0;
    return 2.0 * near_z * far_z / (far_z + near_z - remapped_z * (far_z - near_z));
}

#endif