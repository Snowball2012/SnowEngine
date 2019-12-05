
Texture2D src : register(t0);


float4 main(float4 coord : SV_POSITION) : SV_TARGET
{
    int3 coord2d = int3( coord.x, coord.y, 0 );
    coord2d *= 2;

    float4 s1 = src.Load( coord2d );
    float4 s2 = src.Load( coord2d + int3( 1, 0, 0 ) );
    float4 s3 = src.Load( coord2d + int3( 0, 1, 0 ) );
    float4 s4 = src.Load( coord2d + int3( 1, 1, 0 ) );

    return ( s1 + s2 + s3 + s4 ) * 0.25f;
}
