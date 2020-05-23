#include <engine/stdafx.h>

#include "OrbitCameraController.h"


void OrbitCameraController::Rotate(float yaw, float pitch)
{
	// 1. Save orbit params (to prevent floating point error accumulation from corrupting our orbit)
	DirectX::XMVECTOR orbit_to_pos = DirectX::XMVectorSubtract( m_pos, m_orbit_center );
	if ( ! m_cached_orbit )
	{
		m_cached_orbit.emplace();

		DirectX::XMVECTOR world2local_quat = DirectX::XMVector3Cross( m_eye, DirectX::XMVectorSet( 0, 0, 1, 0 ) );
		world2local_quat.m128_f32[3] = m_eye.m128_f32[2] + 1.0f;
		world2local_quat = DirectX::XMQuaternionNormalize( world2local_quat );

		m_cached_orbit->orbit_to_pos_cs = DirectX::XMVector3Rotate( orbit_to_pos, world2local_quat );
	}

	const DirectX::XMVECTOR eye_dot_up = DirectX::XMVector3Dot( m_eye, m_up );

	float current_pitch = std::asin( eye_dot_up.m128_f32[0] );

	float new_pitch = boost::algorithm::clamp(
		current_pitch + pitch * m_angle_per_unit,
		-DirectX::XM_PIDIV2 + 0.1f,
		DirectX::XM_PIDIV2 - 0.1f );

	float current_yaw = std::atan2( m_eye.m128_f32[2], m_eye.m128_f32[0] );

	m_eye = DirectX::XMLoadFloat3(
		&SphericalToCartesian( 1, DirectX::XM_PIDIV2 - new_pitch, current_yaw + yaw * m_angle_per_unit ) );

	DirectX::XMMATRIX local2world_mat = DirectX::XMMatrixTranspose( DirectX::XMMatrixLookToLH( DirectX::XMVectorZero(), m_eye, m_up ) );
	DirectX::XMVECTOR new_orbit_to_pos = DirectX::XMVector3Transform( m_cached_orbit->orbit_to_pos_cs, local2world_mat );

	m_pos = DirectX::XMVectorAdd( m_orbit_center, new_orbit_to_pos );
}


void OrbitCameraController::Zoom( float units )
{
	DirectX::XMVECTOR orbit_to_pos = DirectX::XMVectorSubtract( m_pos, m_orbit_center );

	float increase = std::powf( m_zoom_per_unit, units );

	m_pos = DirectX::XMVectorMultiplyAdd( orbit_to_pos, DirectX::XMVectorReplicate( increase ), m_orbit_center );
}
