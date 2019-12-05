
Texture2D src : register(t0);

float4 main(float4 coord : SV_POSITION) : SV_TARGET
{
    int2 coord2d = int2(coord.x, coord.y) * 2;

    float4 sample1 = src.Load(int3(coord2d, 0));
    float4 sample2 = src.Load(int3(coord2d + int2(1, 0), 0));
    float4 sample3 = src.Load(int3(coord2d + int2(0, 1), 0));
    float4 sample4 = src.Load(int3(coord2d + int2(1, 1), 0));

    return ( sample1 + sample2 + sample3 + sample4 ) * 0.25;
}