#include "../lib/math_utils.hlsli"

Texture2D src : register(t0);

SamplerState linear_clamp_sampler : register(s0);

float2 random_rotation( float2 seed )
{
    float2 rotation;
    sincos( abs(seed.x + seed.y) * PI / 2.0f , rotation.x, rotation.y );
    return rotation;
}

float2 rotate_direction( float2 direction, float2 rotation )
{
    return float2( dot( rotation, direction ), rotation.x * direction.y - rotation.y * direction.x );
}

float4 main(float4 coord : SV_POSITION, float4 ndc : NDCCOORD) : SV_TARGET
{
    // log average, with blur

    float2 tc = tc_from_ndc( ndc.xy );
    float2 pixel_size_tc;
    src.GetDimensions(pixel_size_tc.x, pixel_size_tc.y);
    pixel_size_tc = rcp( pixel_size_tc ) * 0.5f;

    float4 acc = 0;

    float weight_sum = 0;
    
    const float bias = 1.e-7;

    float2 seed = frac(sin(dot(tc ,float2(12.9898,78.233)*2.0)) * 43758.5453); // random vector for marching directions & pixel position jittering
    float2 rotation = random_rotation( seed );

    for ( int i = -5; i < 5; ++i)
        for (int j = -5; j < 5; ++j)
        {
            float2 offset = float2( 1 + 2 * i, 1 + 2 * j);
            float2 rotated_offset = rotate_direction( offset, rotation );
            float2 normalized_offset = normalize( rotated_offset );
            float angle = asin( normalized_offset.y );
            float2 star_rotation;
            sincos( angle * 2, star_rotation.x, star_rotation.y );


            float star_shaped_weight = pow( 1 - abs( rotate_direction( normalized_offset, star_rotation ).y ), 5 );

            float2 uv = tc + pixel_size_tc*rotated_offset;



            float weight = star_shaped_weight * exp( -(offset.x*offset.x+ offset.y*offset.y) / 100 ) * ( uv.x >= 0 ) * (uv.x <= 1) * (uv.y >= 0) * (uv.y <= 1);
            weight_sum += weight;
            acc += log( src.Sample( linear_clamp_sampler, uv ) + bias ) * weight;
        }

    return exp( acc / weight_sum );
}
