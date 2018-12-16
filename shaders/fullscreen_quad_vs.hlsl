float4 main( uint id : SV_VertexID ) : SV_POSITION
{
	float4 pos;
	if ( id == 0 )
		pos = float4( -1, -1, 0.5, 1 );
	else if ( id == 1 )
		pos = float4( -1, 3, 0.5, 1 );
	else if ( id == 2 )
		pos = float4( 3, -1, 0.5, 1 );
	return pos;
}