// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "GeomGeneration.h"

using namespace DirectX;

void GeomGeneration::MakeLineVertices( const ctTokenLine2d& line, Vertex vertices[4] )
{
    XMFLOAT3 p1( float( line.p1.x ), float( line.p1.y ), 0.f );
    XMFLOAT3 p2( float( line.p2.x ), float( line.p2.y ), 0.f );

    XMFLOAT3 left_dir = p2 - p1;
    std::swap( left_dir.x, left_dir.y );
    left_dir.x *= -1.f;
    XMFloat3Normalize( left_dir );

    left_dir *= float( line.thickness );

    for ( int i : { 0, 1 } )
    {
        vertices[i].pos = p1 + ( i * 2.f - 1 ) * left_dir;
        vertices[i + 2].pos = p2 + ( i * 2.f - 1 ) * left_dir;
    }
}
