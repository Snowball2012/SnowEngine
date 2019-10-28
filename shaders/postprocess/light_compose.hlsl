
Texture2D ambient : register(t0);
Texture2D ssao : register(t1);

SamplerState point_wrap_sampler : register( s0 );

//#define DEBUG_ONLY_DIRECT

float4 main(float4 coord : SV_POSITION) : SV_TARGET
{
    #ifdef DEBUG_ONLY_DIRECT
        return 0;
    #endif

    int2 coord2d = int2(coord.x, coord.y);

    float2 rt_dimensions;
    ambient.GetDimensions( rt_dimensions.x, rt_dimensions.y );

    return ambient.Load(int3(coord2d, 0)) * ssao.Sample( point_wrap_sampler, coord.xy / rt_dimensions ).r;
}