// My implementation of AMD FidelityFX Single Pass Downsampler

static const uint MaxMips = 13;

cbuffer DownsampleSettings : register( b0 )
{
    uint nmips;
    uint ngroups;
    uint2 mip_size[MaxMips];
};

globallycoherent RWTexture2D<float4> mip_dst[MaxMips] : register( u0 );

struct GlobalAtomic
{
   uint counter;
};
globallycoherent RWStructuredBuffer<GlobalAtomic> groups_finished_buf;

groupshared float4 intermediate_data[8][8];
groupshared uint groups_finished;

float4 SampleSourceTex( uint2 pos, uint mip )
{
    float4 res = (float4)0;
    if ( pos.x < mip_size[mip].x && pos.y < mip_size[mip].y )
    {
        res = (float4) 1.0f;
        res.rgb += mip_dst[mip].Load( pos ).rgb;
        res.rgb = log2(res.rgb);        
    }
    return res;
}

void StoreLogSum( float4 log_sum, uint2 pos_global, uint mip )
{
    log_sum /= log_sum.a;
    mip_dst[mip + 1][pos_global] = exp2( log_sum ) - 1.0f;
}

void DownsampleMipChain( uint morton_idx, uint2 src_idx, uint2 idx_in_group, uint src_mip )
{
    // downsample 6 mips
    // 0 -> 1 mip, reduce 4 quads
    float4 temps[4];
    float4 log_sum = (float4) 0;
    log_sum += SampleSourceTex( src_idx, src_mip ) * 0.25f;
    log_sum += SampleSourceTex( src_idx + uint2( 0, 1 ), src_mip ) * 0.25f;
    log_sum += SampleSourceTex( src_idx + uint2( 1, 0 ), src_mip ) * 0.25f;
    log_sum += SampleSourceTex( src_idx + uint2( 1, 1 ), src_mip ) * 0.25f;
    StoreLogSum( log_sum, src_idx / 2, src_mip );
    temps[0] = log_sum;
    
    log_sum = 0;
    log_sum += SampleSourceTex( src_idx + uint2( 2, 0 ), src_mip ) * 0.25f;
    log_sum += SampleSourceTex( src_idx + uint2( 3, 0 ), src_mip ) * 0.25f;
    log_sum += SampleSourceTex( src_idx + uint2( 2, 1 ), src_mip ) * 0.25f;
    log_sum += SampleSourceTex( src_idx + uint2( 3, 1 ), src_mip ) * 0.25f;
    StoreLogSum( log_sum, src_idx / 2 + uint2( 1, 0 ), src_mip );
    temps[1] = log_sum;
    
    log_sum = 0;
    log_sum += SampleSourceTex( src_idx + uint2( 0, 2 ), src_mip ) * 0.25f;
    log_sum += SampleSourceTex( src_idx + uint2( 1, 2 ), src_mip ) * 0.25f;
    log_sum += SampleSourceTex( src_idx + uint2( 0, 3 ), src_mip ) * 0.25f;
    log_sum += SampleSourceTex( src_idx + uint2( 1, 3 ), src_mip ) * 0.25f;
    StoreLogSum( log_sum, src_idx / 2 + uint2( 0, 1 ), src_mip );
    temps[2] = log_sum;
    
    log_sum = 0;
    log_sum += SampleSourceTex( src_idx + uint2( 2, 2 ), src_mip ) * 0.25f;
    log_sum += SampleSourceTex( src_idx + uint2( 3, 2 ), src_mip ) * 0.25f;
    log_sum += SampleSourceTex( src_idx + uint2( 2, 3 ), src_mip ) * 0.25f;
    log_sum += SampleSourceTex( src_idx + uint2( 3, 3 ), src_mip ) * 0.25f;
    StoreLogSum( log_sum, src_idx / 2 + uint2( 0, 1 ), src_mip );
    temps[3] = log_sum;
    
    src_mip++;
    
    if ( src_mip >= nmips )
        return;
    
    src_idx /= 4;
    
    // 1 -> 2 mip, reduce 4 temps
    log_sum = 0;
    log_sum += temps[0] * 0.25f;
    log_sum += temps[1] * 0.25f;
    log_sum += temps[2] * 0.25f;
    log_sum += temps[3] * 0.25f;
    StoreLogSum( log_sum, src_idx, src_mip );
    
    src_mip++;
    if ( src_mip >= nmips )
        return;
    
    // 2 -> 3, use wave intrisics to reduce
    src_idx /= 2;
    log_sum *= 0.25f;
    if (morton_idx % 4 == 0)
    {
        log_sum += QuadReadLaneAt( log_sum, 1 );
        log_sum += QuadReadLaneAt( log_sum, 2 );
        log_sum += QuadReadLaneAt( log_sum, 3 );
        StoreLogSum( log_sum, src_idx, src_mip );
        intermediate_data[idx_in_group.x / 2][idx_in_group.y / 2] = log_sum;
    }
    
    src_mip++;
    if ( src_mip >= nmips )
        return;
    
    // 3 -> 4
    if ( morton_idx >= 64 )
        return;
    
    GroupMemoryBarrierWithGroupSync();
    src_idx /= 2;
    log_sum = intermediate_data[idx_in_group.x][idx_in_group.y];
    log_sum *= 0.25f;
    if ( morton_idx % 4 == 0 )
    {
        log_sum += QuadReadLaneAt( log_sum, 1 );
        log_sum += QuadReadLaneAt( log_sum, 2 );
        log_sum += QuadReadLaneAt( log_sum, 3 );
        StoreLogSum( log_sum, src_idx, src_mip );
        intermediate_data[idx_in_group.x / 2][idx_in_group.y / 2] = log_sum;        
    }
    
    src_mip++;
    if ( src_mip >= nmips )
        return;
    
    // 4 -> 5
    if ( morton_idx >= 16 )
        return;
    
    GroupMemoryBarrierWithGroupSync();
    src_idx /= 2;
    log_sum = intermediate_data[idx_in_group.x][idx_in_group.y];
    log_sum *= 0.25f;
    if ( morton_idx % 4 == 0 )
    {
        log_sum += QuadReadLaneAt( log_sum, 1 );
        log_sum += QuadReadLaneAt( log_sum, 2 );
        log_sum += QuadReadLaneAt( log_sum, 3 );
        StoreLogSum( log_sum, src_idx, src_mip );
        intermediate_data[idx_in_group.x / 2][idx_in_group.y / 2] = log_sum;        
    }
    
    src_mip++;
    if ( src_mip >= nmips )
        return;
    
    // 5 -> 6
    if ( morton_idx >= 4 )
        return;
    
    GroupMemoryBarrierWithGroupSync();
    src_idx /= 2;
    log_sum = intermediate_data[idx_in_group.x][idx_in_group.y];
    log_sum *= 0.25f;
    if ( morton_idx == 0 )
    {
        log_sum += QuadReadLaneAt( log_sum, 1 );
        log_sum += QuadReadLaneAt( log_sum, 2 );
        log_sum += QuadReadLaneAt( log_sum, 3 );
        StoreLogSum( log_sum, src_idx, src_mip );  
    }   
}


[numthreads(256, 1, 1)]
void main( uint morton_idx : SV_GroupIndex, uint3 group_idx : SV_GroupID )
{
    uint2 src_idx;
    // demortonize
    {
        uint x = ( ( morton_idx & 0xaa ) << 7 ) | ( morton_idx & 0x55 );
        x = ( x | ( x >> 1 ) ) & 0x3333;
        x = ( x | ( x >> 2 ) ) & 0x0f0f;
        x = ( x | ( x >> 4 ) ) & 0x00ff;
        src_idx.x = x & 0xf;
        src_idx.y = ( x & 0xf0 ) >> 4;
    }
    
    DownsampleMipChain( morton_idx, src_idx * 4 + group_idx.xy * 64, src_idx, 0 );
    
    if ( nmips <= 7 )
        return;
    
    if ( morton_idx == 0 )
        InterlockedAdd( groups_finished_buf[0].counter, 1, groups_finished );
        
    GroupMemoryBarrierWithGroupSync();
    if ( groups_finished != ngroups - 1 )
        return;
    
    DownsampleMipChain( morton_idx, src_idx, src_idx, 6 );
}