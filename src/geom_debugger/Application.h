#pragma once

#include <GameTimer.h>

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

	struct FrameStats
	{
		uint64_t frame_cnt = 0;
		double time_elapsed = 0.0;
	} m_frame_stats;

    void UpdateFrameStats( const GameTimer& timer, FrameStats& stats );

	void OnResize() {}
	void Update( const GameTimer& timer ) {}
	void Draw( const GameTimer& timer ) {}

	void OnMouseDown( WPARAM btnState, int x, int y ) {}
	void OnMouseUp( WPARAM btnState, int x, int y ) {}
	void OnMouseMove( WPARAM btnState, int x, int y ) {}
};