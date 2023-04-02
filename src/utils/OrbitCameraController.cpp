// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <snow_engine/stdafx.h>

#include "OrbitCameraController.h"


void OrbitCameraController::Rotate(float yaw, float pitch)
{
	// 1. Save orbit params (to prevent floating point error accumulation from corrupting our orbit)
	DirectX::XMVECTOR orbit_to_pos = DirectX::XMVectorSubtract( m_pos, m_orbit_center );
	if ( ! m_cached_orbit )
	{
		m_cached_orbit.emplace();

		DirectX::XMMATRIX world2local_mat = DirectX::XMMatrixLookToLH( DirectX::XMVectorZero(), m_eye, m_up );
		m_cached_orbit->orbit_to_pos_cs = DirectX::XMVector3Transform( orbit_to_pos, world2local_mat );
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

	if ( m_cached_orbit )
		m_cached_orbit->orbit_to_pos_cs = DirectX::XMVectorMultiply( m_cached_orbit->orbit_to_pos_cs, DirectX::XMVectorReplicate( increase ) );

	m_pos = DirectX::XMVectorMultiplyAdd( orbit_to_pos, DirectX::XMVectorReplicate( increase ), m_orbit_center );
}
