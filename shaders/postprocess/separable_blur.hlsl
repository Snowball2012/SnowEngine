
Texture2D input : register( t0 );
Texture2D depth : register( t1 );

RWTexture2D<float> output : register( u0 );

[numthreads(64, 4, 1)]
void main( uint3 thread : SV_DispatchThreadID )
{
    // we're fine with a potential underflow, reads from inaccessible mem return 0 anyway
    const int noffsets = 9;
    int2 offsets[noffsets] = 
    {
        int2( -1, -1 ), int2(  0, -1 ), int2(  1, -1 ),
        int2( -1,  0 ), int2(  0,  0 ), int2(  1,  0 ),
        int2( -1,  1 ), int2(  0,  1 ), int2(  1,  1 ),
    };

    min16float sum = 0.0f;
    [unroll]
    for ( int i = 0; i < noffsets; ++i )
        sum += input[thread.xy + offsets[i]].r;

    output[thread.xy] = sum / min16float( noffsets );
}