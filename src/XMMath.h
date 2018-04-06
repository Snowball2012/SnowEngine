#pragma once

#include <DirectXMath.h>

DirectX::XMFLOAT3 operator-( const DirectX::XMFLOAT3& lhs, const DirectX::XMFLOAT3& rhs );
DirectX::XMFLOAT3 operator+( const DirectX::XMFLOAT3& lhs, const DirectX::XMFLOAT3& rhs );
DirectX::XMFLOAT3& operator+=( DirectX::XMFLOAT3& op, const DirectX::XMFLOAT3& rhs );
DirectX::XMFLOAT3 operator*( const DirectX::XMFLOAT3& lhs, float mod );
DirectX::XMFLOAT3 operator*( float mod, const DirectX::XMFLOAT3& op );
DirectX::XMFLOAT3& operator*=( DirectX::XMFLOAT3& op, float mod );
DirectX::XMFLOAT3 operator/( const DirectX::XMFLOAT3& lhs, float mod );

float XMFloat3LenSquared( const DirectX::XMFLOAT3& op );
float XMFloat3Normalize( DirectX::XMFLOAT3& lhs );

DirectX::XMFLOAT3 SphericalToCartesian( float radius, float phi, float theta );

DirectX::XMMATRIX InverseTranspose( DirectX::CXMMATRIX m );