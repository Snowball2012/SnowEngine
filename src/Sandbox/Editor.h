#pragma once

#include "StdAfx.h"

#include <Engine/LevelObjects.h>

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

class LevelEditor
{
private:

	std::vector<std::unique_ptr<LevelObject>> m_level_objects;

	int m_selected_object = -1;

	int m_mouse_hover_picking_id = -1;

	EditorCamera m_editor_camera;

	std::unique_ptr<World> m_world;

	std::unique_ptr<Scene> m_scene;
	std::unique_ptr<SceneView> m_scene_view;

public:
	LevelEditor();
	~LevelEditor();

	bool OpenLevel( const char* filepath );
	bool SaveLevel( const char* filepath ) const;

	bool Update( float delta_time_sec );
	bool Draw( Rendergraph& framegraph, ISceneRenderExtension* required_extension, RHIBuffer* readback_buffer );

	void UpdateReadback( const ViewFrameReadbackData& readback_data );

	bool SetViewportExtents( glm::uvec2 extents );

	void ResetAccumulation();

	// @todo: can't return const world because const views are not supported in ecs yet
	World* GetWorld() const { return m_world.get(); }

private:

	void UpdateCamera( float delta_time_sec );

	void UpdateScene();

	bool HandleMousePicking();
};