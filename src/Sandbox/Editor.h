#pragma once

#include "StdAfx.h"

#include <Engine/LevelObjects.h>

class Editor
{
private:
public:
};


struct EditorCameraCommands
{
	bool forward = false;
	bool back = false;
	bool move_left = false;
	bool move_right = false;
	bool up = false;
	bool down = false;
};

class EditorCamera
{
private:
	glm::vec3 m_cam_pos = glm::vec3( 2, 2, 2 );
	glm::vec2 m_cam_angles_radians = glm::vec2( -2.43f, 2.15f );
	glm::quat m_cam_orientation = IdentityQuat();
	float m_cam_turn_speed = 0.005f;
	float m_cam_move_speed = 1.0f;
	float m_fov_degrees = 45.0f;

	mutable bool m_camera_moved = false; // @todo: remove mutable, return move flag in Update

public:

	void SetupView( SceneView& view ) const;

	// returns true if camera has been changed
	bool UpdateGUI();

	void Update( float delta_time );
};