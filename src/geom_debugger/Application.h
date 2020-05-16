#pragma once

#include <GameTimer.h>
#include <engine/OldRenderer.h>

#include <Windows.h>

#include <string>

class Application
{
public:
	Application( HINSTANCE instance, LPSTR cmd_line );

	bool Initialize();
	int Run();

	LRESULT MainWndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );
private:
	HINSTANCE m_app_instance = nullptr;
	HWND m_main_wnd = nullptr;

	GameTimer m_timer;

	uint32_t m_main_width = 800;
	uint32_t m_main_height = 600;

	bool m_paused = false;
	bool m_resizing = false;
	bool m_maximized = false;
	bool m_minimized = false;

	std::string m_cmd_line;

	std::unique_ptr<OldRenderer> m_renderer;

	struct SceneAssets
	{
		EnvMapID skybox = EnvMapID::nullid;
		MaterialID default_material = MaterialID::nullid;
		StaticMeshID cube = StaticMeshID::nullid;
		StaticSubmeshID cube_submesh = StaticSubmeshID::nullid;
	} m_assets;

	struct SceneObjects
	{
		CameraID main_camera = CameraID::nullid;
		World::Entity demo_cube = World::Entity::nullid;
	} m_sceneobjects;

	struct FrameStats
	{
		uint64_t frame_cnt = 0;
		double time_elapsed = 0.0;
	} m_frame_stats;

    void UpdateFrameStats( const GameTimer& timer, FrameStats& stats );

	void OnResize();
	void Update( const GameTimer& timer ) {}
	void Draw(const GameTimer& timer);

	void OnMouseDown( WPARAM btnState, int x, int y ) {}
	void OnMouseUp( WPARAM btnState, int x, int y ) {}
	void OnMouseMove( WPARAM btnState, int x, int y ) {}

	bool InitSceneAssets( SceneClientView& renderer_scene, SceneAssets& assets ) const;
	bool InitSceneObjects( const SceneAssets& assets, SceneClientView& renderer_scene, SceneObjects& objects ) const;
};