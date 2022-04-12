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

static constexpr DirectX::XMFLOAT4X4 Identity4x4 =
{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
};

static constexpr float HaltonSequence23[8][2] =
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

struct RayTriangleIntersection
{
	// x, y - u, v triangle barycentrics
	// z - t
	// w - intersection matrix determinant
	DirectX::XMVECTOR coords;
	bool IsDegenerate( float tolerance ) const { return std::abs( coords.m128_f32[3] ) <= tolerance; }
	bool HitDetected(
		float onplane_tolerance, float rayorigin_tolerance, float triangle_tolerance ) const
	{
		return !IsDegenerate( onplane_tolerance )
			&& coords.m128_f32[2] >= -rayorigin_tolerance
			&& coords.m128_f32[0] >= -triangle_tolerance
			&& coords.m128_f32[1] >= -triangle_tolerance
			&& coords.m128_f32[0] <= 1.0f + triangle_tolerance
			&& coords.m128_f32[1] <= 1.0f + triangle_tolerance
			&& ( coords.m128_f32[0] + coords.m128_f32[1] ) <= 1.0f + triangle_tolerance;
	}
	bool RayEntersTriangle() const { return coords.m128_f32[3] > 0; } // otherwise exits
	bool Validate(
		const float v1[3], const float v2[3], const float v3[3],
		const float ray_origin[3], const float ray_direction[3],
		float tolerance ) const; // for convinient testing
};

inline uint64_t CalcAlignedSize( uint64_t size, uint64_t alignment )
{
	if ( alignment > 1 )
		return ( size + ( alignment - 1 ) ) & ~( alignment - 1 );
	return size;
}

RayTriangleIntersection IntersectRayTriangle(
	const float v1[3], const float v2[3], const float v3[3],
	const float ray_origin[3], const float ray_direction[3]);