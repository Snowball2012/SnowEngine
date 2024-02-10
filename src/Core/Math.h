#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

constexpr glm::vec3 ZeroVec3() { return glm::zero<glm::vec3>(); }
constexpr glm::vec3 OneVec3() { return glm::one<glm::vec3>(); }
constexpr glm::quat IdentityQuat() { return glm::quat_identity<float, glm::defaultp>(); }

struct Transform
{
	glm::vec3 translation = ZeroVec3();
	glm::quat orientation = IdentityQuat();
	glm::vec3 scale = OneVec3();
};

inline glm::mat4x4 ToMatrix4x4( const Transform& tf )
{
	glm::mat4x4 m;
	m = glm::scale( glm::identity<glm::mat4>(), tf.scale );
	m = glm::toMat4( tf.orientation ) * m;
	m = glm::translate( glm::identity<glm::mat4>(), tf.translation ) * m;

	return m;
}

inline glm::mat3x4 ToMatrixRowMajor3x4( const Transform& tf )
{
	glm::mat4x4 m = ToMatrix4x4( tf );
	glm::mat4x4 m_row_major = glm::transpose( m );

	return glm::mat3x4( m_row_major[0], m_row_major[1], m_row_major[2] );
}

inline glm::vec3 SphericalToCartesian( float radius, float phi, float theta )
{
	glm::vec3 res;

	res.x = radius * sinf( theta ) * cosf( phi );
	res.z = radius * sinf( theta ) * sinf( phi );
	res.y = radius * cosf( theta );
	return res;
}

// https://github.com/skeeto/hash-prospector
inline uint32_t HashUint32( uint32_t x )
{
	x ^= x >> 16;
	x *= 0xa812d533;
	x ^= x >> 15;
	x *= 0xb278e4ad;
	x ^= x >> 17;
	return x;
}

inline float Wrap( float x, float min, float max )
{
	if ( x > max || x < min )
	{
		float range = max - min;
		return glm::fract( ( x - min ) / range ) * range + min;
	}
	return x;
}
