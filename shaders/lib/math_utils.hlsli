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

// from Unreal Engine path tracer
float4 uniform_sample_cone(float2 rand_value, float sin_theta_max2)
{
    float Phi = 2 * PI * rand_value.x;
    // The expression 1-sqrt(1-x) is susceptible to catastrophic cancelation.
    // Instead, use a series expansion about 0 which is accurate within 10^-7
    // and much more numerically stable.
    float OneMinusCosThetaMax = sin_theta_max2 < 0.01 ? sin_theta_max2 * (0.5 + 0.125 * sin_theta_max2) : 1 - sqrt(1 - sin_theta_max2);

    float CosTheta = 1 - OneMinusCosThetaMax * rand_value.y;
    float SinTheta = sqrt(1 - CosTheta * CosTheta);

    float3 L;
    L.x = SinTheta * cos(Phi);
    L.y = SinTheta * sin(Phi);
    L.z = CosTheta;
    float PDF = 1.0 / (2 * PI * OneMinusCosThetaMax);

    return float4(L, PDF);
}

float3 apply_cone_sample_to_direction(float3 sample, float3 dir)
{
    float3 cross_product = cross(float3(0,0,1), dir);
    float sin_a = length(cross_product);
    float cos_a = sqrt(1 - sqr(sin_a));
    float3 axis = cross_product / sin_a;

    // Rodrigues' rotation formula
    float3 rotated = sample * cos_a + sin(cross(axis, sample)) + (1 - cos_a) * dot(axis, sample) * axis;
    return rotated;
}

#endif