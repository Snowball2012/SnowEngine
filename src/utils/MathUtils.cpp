// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "MathUtils.h"

#include <DirectXCollision.h>

using namespace DirectX;

bool operator==( const XMFLOAT2& lhs, const XMFLOAT2& rhs )
{
	return ( lhs.x == rhs.x ) && ( lhs.y == rhs.y );
}

bool operator==( const XMFLOAT3& lhs, const XMFLOAT3& rhs )
{
	return ( lhs.x == rhs.x ) && ( lhs.y == rhs.y ) && ( lhs.z == rhs.z );
}

XMFLOAT2 operator-( const XMFLOAT2& lhs, const XMFLOAT2& rhs )
{
    return  XMFLOAT2( lhs.x - rhs.x, lhs.y - rhs.y );
}

XMFLOAT3 operator-( const  XMFLOAT3& lhs, const  XMFLOAT3& rhs )
{
    return  XMFLOAT3( lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z );
}

XMFLOAT3 operator+( const  XMFLOAT3& lhs, const  XMFLOAT3& rhs )
{
    return  XMFLOAT3( lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z );
}

XMFLOAT3& operator+=( XMFLOAT3& op, const XMFLOAT3& rhs )
{
    op = op + rhs;
    return op;
}

XMFLOAT3 operator*( const  XMFLOAT3& lhs, float mod )
{
    return  XMFLOAT3( lhs.x * mod, lhs.y * mod, lhs.z * mod );
}

XMFLOAT3 operator*( float mod, const  XMFLOAT3& op )
{
    return op*mod;
}

XMFLOAT3& operator*=(  XMFLOAT3& op, float mod )
{
    op = mod * op;
    return op;
}

XMFLOAT3 operator/( const XMFLOAT3& lhs, float mod )
{
    return  XMFLOAT3( lhs.x / mod, lhs.y / mod, lhs.z / mod );
}

float XMFloat3LenSquared( const XMFLOAT3& op )
{
    return op.x * op.x + op.y * op.y + op.z * op.z;
}

float XMFloat3Normalize( XMFLOAT3& lhs )
{
    float res = sqrt( XMFloat3LenSquared( lhs ) );
    lhs = lhs / res;
    return res;
}

XMFLOAT3 SphericalToCartesian( float radius, float phi, float theta )
{
    XMFLOAT3 res;

    res.x = radius * sinf( phi ) * cosf( theta );
    res.z = radius * sinf( phi ) * sinf( theta );
    res.y = radius * cosf( phi );
    return res;
}

XMMATRIX InverseTranspose( DirectX::CXMMATRIX m )
{
    XMMATRIX zeroed_translation = m;
    zeroed_translation.r[3] = XMVectorSet( 0, 0, 0, 1 );
    XMVECTOR det = XMMatrixDeterminant( zeroed_translation );
    return XMMatrixTranspose( XMMatrixInverse( &det, zeroed_translation ) );
}

float DistanceToBoxSqr( const XMVECTOR& point, const BoundingOrientedBox& box )
{
    float dist2 = 0.0f;

    XMVECTOR pt2center = XMVectorSubtract( point, XMLoadFloat3( &box.Center ) );

    for ( int i : { 0, 1, 2 } )
    {
        XMVECTOR ort = XMVectorZero();
        ort = XMVectorSetByIndex( ort, 1.0f, i );
        XMVector3Rotate( ort, XMLoadFloat4( &box.Orientation ) );
        const float dist_component = std::abs( XMVectorGetX( XMVector3Dot( pt2center, ort ) ) );

        const float extent = reinterpret_cast<const float*>( &box.Extents )[i]; // !!! HACK HACK HACK !!! too bad XMFLOAT3 doesn't have index operator
        if ( dist_component > extent )
            dist2 += squared( dist_component - extent );
    }
    
    return dist2;
}

RayTriangleIntersection IntersectRayTriangle(
	const float v1[3], const float v2[3], const float v3[3],
	const float ray_origin[3], const float ray_direction[3])
{
	// a, b, c - triangle vertices
	// d - ray direction
	// o - ray origin
	// q - intersection matrix
	//
	// q = |c - a|
	//	   |c - b|
	//	   |  d  |
	//
	// [u v t] * q = [c - o]
	// [u v t] = [c - o] * q_inv;

	XMVECTOR c = XMLoadFloat3( &XMFLOAT3( v3 ) );

	XMMATRIX q;
	q.r[0] = c - XMLoadFloat3( &XMFLOAT3( v1 ) );
	q.r[1] = c - XMLoadFloat3( &XMFLOAT3( v2 ) );
	q.r[2] = XMLoadFloat3( &XMFLOAT3( ray_direction ) );
	q.r[3] = XMVectorSetW( XMVectorZero(), 1 );

	XMVECTOR q_det;
	XMMATRIX q_inv = XMMatrixInverse( &q_det, q );

	RayTriangleIntersection res;
	res.coords = c - XMLoadFloat3( &XMFLOAT3( ray_origin ) );
	res.coords = XMVector4Transform( res.coords, q_inv );
	res.coords.m128_f32[3] = q_det.m128_f32[0];

	return res;
}
