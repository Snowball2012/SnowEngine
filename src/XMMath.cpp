#include "stdafx.h"

#include "XMMath.h"

using namespace DirectX;

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

DirectX::XMMATRIX InverseTranspose( DirectX::CXMMATRIX m )
{
	XMMATRIX zeroed_translation = m;
	zeroed_translation.r[3] = XMVectorSet( 0, 0, 0, 1 );
	XMVECTOR det = XMMatrixDeterminant( zeroed_translation );
	return XMMatrixTranspose( XMMatrixInverse( &det, zeroed_translation ) );
}
