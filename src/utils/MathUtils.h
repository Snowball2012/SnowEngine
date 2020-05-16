#pragma once

#include <d3d12.h>
#include <dxtk12/SimpleMath.h>

template<class T>
constexpr T squared( T val ) { return val * val; }

template<class T>
constexpr T ceil_integer_div( T val, T divisor ) { return ( val + divisor - 1 ) / divisor; }

template<class T>
constexpr T lerp( T v0, T v1, T t ) { return std::fma( t, v1, std::fma( -t, v0, v0 ) ); }

constexpr uint32_t upper_power_of_two(uint32_t v)
{
    assert( v != 0 );
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

constexpr uint64_t upper_power_of_two(uint64_t v)
{
    assert( v != 0 );
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    return v;
}

inline bool is_close(float lhs, float rhs, float eps)
{
	return std::abs(lhs - rhs) <= eps;
}

bool operator==( const DirectX::XMFLOAT2& lhs, const DirectX::XMFLOAT2& rhs );
DirectX::XMFLOAT2 operator-( const DirectX::XMFLOAT2& lhs, const DirectX::XMFLOAT2& rhs );

bool operator==( const DirectX::XMFLOAT3& lhs, const DirectX::XMFLOAT3& rhs );
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

// returns 0 if a point is inside a box
float DistanceToBoxSqr( const DirectX::XMVECTOR& point, const DirectX::BoundingOrientedBox& box );

constexpr DirectX::XMFLOAT4X4 Identity4x4 =
{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
};

constexpr float HaltonSequence23[8][2] =
{
    { 1.f / 2.f, 1.f / 3.f },
    { 1.f / 4.f, 2.f / 3.f },
    { 3.f / 4.f, 1.f / 9.f },
    { 1.f / 8.f, 4.f / 9.f },
    { 5.f / 8.f, 7.f / 9.f },
    { 3.f / 8.f, 2.f / 9.f },
    { 7.f / 8.f, 5.f / 9.f },
    { 1.f / 16.f, 8.f / 9.f }
};