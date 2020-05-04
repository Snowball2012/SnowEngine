struct VertexOut
{
    float4 pos : SV_POSITION;
    float4 pos_ndc : NDCCOORD;
    uint split_idx : SV_RenderTargetArrayIndex;
};

VertexOut main( uint id : SV_VertexID )
{
    VertexOut res;
    if ( id == 0 )
        res.pos = float4( -1, -1, 0.5f, 1 );
    else if ( id == 1 )
        res.pos = float4( -1, 3, 0.5f, 1 );
    else
        res.pos = float4( 3, -1, 0.5f, 1 );
    res.pos_ndc = res.pos;
    res.split_idx = 0;
    return res;
}