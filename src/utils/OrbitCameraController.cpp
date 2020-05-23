#include <engine/stdafx.h>

#include "OrbitCameraController.h"


void OrbitCameraController::Rotate(float yaw, float pitch)
{
	const DirectX::XMVECTOR eye_dot_up = DirectX::XMVector3Dot( m_eye, m_up );

	float current_pitch = std::asin( eye_dot_up.m128_f32[0] );

	float new_pitch = boost::algorithm::clamp(
		current_pitch + pitch * m_angle_per_unit,
		-DirectX::XM_PIDIV2 + 0.1f,
		DirectX::XM_PIDIV2 - 0.1f );

	float current_yaw = std::atan2( m_eye.m128_f32[2], m_eye.m128_f32[0] );

	DirectX::XMVECTOR new_eye = DirectX::XMLoadFloat3(
		&SphericalToCartesian( 1, DirectX::XM_PIDIV2 - new_pitch, current_yaw + yaw * m_angle_per_unit ) );

	DirectX::XMVECTOR rotation_quat = DirectX::XMVector3Cross( m_eye, new_eye );
	rotation_quat.m128_f32[3] = DirectX::XMVector3Dot( new_eye, m_eye ).m128_f32[0];

	DirectX::XMQuaternionNormalize( rotation_quat );

	m_eye = DirectX::XMVector3Rotate( m_eye, rotation_quat );
	m_eye = DirectX::XMVector3Normalize( m_eye );

	DirectX::XMVECTOR orbit_to_pos = DirectX::XMVectorSubtract( m_pos, m_orbit_center );
	DirectX::XMVECTOR new_orbit_to_pos = DirectX::XMVector3Rotate( orbit_to_pos, rotation_quat );

	m_pos = DirectX::XMVectorAdd( m_orbit_center, new_orbit_to_pos );
}


void OrbitCameraController::Zoom( float units )
{
	DirectX::XMVECTOR orbit_to_pos = DirectX::XMVectorSubtract( m_pos, m_orbit_center );

	float increase = std::powf( m_zoom_per_unit, units );

	m_pos = DirectX::XMVectorMultiplyAdd( orbit_to_pos, DirectX::XMVectorReplicate( increase ), m_orbit_center );
}
