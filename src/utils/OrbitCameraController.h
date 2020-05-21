#pragma once

#include <utils/MathUtils.h>

class OrbitCameraController
{
	DirectX::XMVECTOR m_pos;
	DirectX::XMVECTOR m_up;
	DirectX::XMVECTOR m_eye;
	DirectX::XMVECTOR m_orbit_center;

	float m_angle_per_unit;
	float m_zoom_per_unit;

public:
	const DirectX::XMVECTOR& GetPosition() const { return m_pos; }
	const DirectX::XMVECTOR& GetUp() const { return m_up; }
	const DirectX::XMVECTOR& GetEyeDir() const { return m_eye; }

	void SetPosition( const float pos[3] ) { m_pos = DirectX::XMLoadFloat3( &DirectX::XMFLOAT3( pos ) ); }
	void SetEye( const float eye[3] ) { m_eye = DirectX::XMLoadFloat3( &DirectX::XMFLOAT3( eye ) ); }
	void SetUp( const float up[3] ) { m_up = DirectX::XMLoadFloat3( &DirectX::XMFLOAT3( up ) ); }
	void SetOrbitPoint( const float point[3] ) { m_orbit_center = DirectX::XMLoadFloat3( &DirectX::XMFLOAT3( point ) ); }
	void SetAngleSpeed( float angle_per_unit ) { m_angle_per_unit = angle_per_unit; }
	void SetZoomSpeed( float increase_per_unit ) { m_zoom_per_unit = increase_per_unit; }

	void Rotate( float yaw, float pitch );
	void Zoom( float units ); // positive - increase distance (zoom out), negative - decrease distance (zoom in)
};