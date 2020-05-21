#include <engine/stdafx.h>

#include "OrbitCameraController.h"


void OrbitCameraController::Rotate(float yaw, float pitch)
{
	const DirectX::XMVECTOR eye_dot_up = DirectX::XMVector3Dot( m_eye, m_up );

	float current_pitch = std::asin( eye_dot_up.m128_f32[0] );

	float new_pitch = boost::algorithm::clamp(
		current_pitch + pitch * m_angle_per_unit,
		-DirectX::XM_PIDIV2 + 10 * FLT_EPSILON,
		DirectX::XM_PIDIV2 - 10 * FLT_EPSILON );

	DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationRollPitchYaw( new_pitch - current_pitch, yaw * m_angle_per_unit, 0 );

	m_eye = DirectX::XMVector4Transform( m_eye, rotation );
	DirectX::XMVECTOR orbit_to_pos = DirectX::XMVectorSubtract( m_pos, m_orbit_center );
	DirectX::XMVECTOR new_orbit_to_pos = DirectX::XMVector4Transform( orbit_to_pos, rotation );

	m_pos = DirectX::XMVectorAdd( m_orbit_center, new_orbit_to_pos );
}


void OrbitCameraController::Zoom( float units )
{
	DirectX::XMVECTOR orbit_to_pos = DirectX::XMVectorSubtract( m_pos, m_orbit_center );

	float increase = std::powf( m_zoom_per_unit, units );

	m_pos = DirectX::XMVectorMultiplyAdd( orbit_to_pos, DirectX::XMVectorReplicate( increase ), m_orbit_center );
}
