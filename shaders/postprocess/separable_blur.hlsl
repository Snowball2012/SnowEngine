
Texture2D input : register( t0 );
Texture2D depth : register( t1 );

RWTexture2D<min16float> output : register( u0 );

[numthreads(64, 4, 1)]
void main( uint3 thread : SV_DispatchThreadID )
{
    // we're fine with a potential underflow, reads from inaccessible mem return 0 anyway
    const int noffsets = 25;
    int2 offsets[noffsets] = 
    {
        int2( -2, -2 ), int2( -1, -2 ), int2(  0, -2 ), int2(  1, -2 ), int2( 2, -2 ),
        int2( -2, -1 ), int2( -1, -1 ), int2(  0, -1 ), int2(  1, -1 ), int2( 2, -1 ),
        int2( -2,  0 ), int2( -1,  0 ), int2(  0,  0 ), int2(  1,  0 ), int2( 2,  0 ),
        int2( -2,  1 ), int2( -1,  1 ), int2(  0,  1 ), int2(  1,  1 ), int2( 2,  1 ),
        int2( -2,  2 ), int2( -1,  2 ), int2(  0,  2 ), int2(  1,  2 ), int2( 2,  2 )
    };

    float weights[noffsets] = 
    {
        0.003765f,	0.015019f,	0.023792f,	0.015019f,	0.003765f,
        0.015019f,	0.059912f,	0.094907f,	0.059912f,	0.015019f,
        0.023792f,	0.094907f,	0.150342f,	0.094907f,	0.023792f,
        0.015019f,	0.059912f,	0.094907f,	0.059912f,	0.015019f,
        0.003765f,	0.015019f,	0.023792f,	0.015019f,	0.003765f
    };

    min16float sum = 0.0f;
    [unroll]
    for ( int i = 0; i < noffsets; ++i )
        sum += input[thread.xy + offsets[i]].r * weights[i];

    output[thread.xy] = saturate( sum );
}