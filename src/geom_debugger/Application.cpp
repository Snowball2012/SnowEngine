#include "stdafx.h"

#include "Application.h"

#include <optick.h>
#include <imgui/imgui.h>

#include <WindowsX.h>

#include <assert.h>

namespace
{
	LRESULT CALLBACK MainWndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
	{
		// Forward to main app WndProc
		Application* app = nullptr;
		switch ( msg )
		{

			// WM_ACTIVATE is sent when the window is activated or deactivated.  
			// We pause the game when the window is deactivated and unpause it 
			// when it becomes active.  
			case WM_NCCREATE:
			{
				CREATESTRUCT* createstruct = reinterpret_cast<CREATESTRUCT*>( lParam );
				assert( createstruct != nullptr );
				if ( ! createstruct )
					return FALSE;

				app = reinterpret_cast<Application*>( createstruct->lpCreateParams );
				SetWindowLongPtr( hwnd, GWLP_USERDATA, LONG_PTR( app ) );
				break;
			}
			default:
			{
				app = reinterpret_cast<Application*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
				break;
			}
		}

		if ( app )
			return app->MainWndProc( hwnd, msg, wParam, lParam );

		return DefWindowProc( hwnd, msg, wParam, lParam );
	}
}

Application::Application( HINSTANCE instance, LPSTR cmd_line )
	: m_app_instance( instance ), m_cmd_line( cmd_line )
{
}


bool Application::Initialize()
{
    WNDCLASS wc;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = ::MainWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = m_app_instance;
    wc.hIcon = LoadIcon( 0, IDI_APPLICATION );
    wc.hCursor = LoadCursor( 0, IDC_ARROW );
    wc.hbrBackground = (HBRUSH)GetStockObject( NULL_BRUSH );
    wc.lpszMenuName = 0;
    wc.lpszClassName = L"MainWnd";

    if ( !RegisterClass( &wc ) )
    {
        MessageBox( 0, L"RegisterClass Failed.", 0, 0 );
        return false;
    }

    // Compute window rectangle dimensions based on requested client area dimensions.
    RECT R = { 0, 0, LONG( m_main_width ), LONG( m_main_height ) };
    AdjustWindowRect( &R, WS_OVERLAPPEDWINDOW, false );
    int width = R.right - R.left;
    int height = R.bottom - R.top;

    m_main_wnd = CreateWindow( L"MainWnd", L"Geom Debugger",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, m_app_instance, this );

    if ( !m_main_wnd )
    {
        MessageBox( 0, L"CreateWindow Failed.", 0, 0 );
        return false;
    }

    ShowWindow( m_main_wnd, SW_SHOW );
    UpdateWindow( m_main_wnd );


	m_renderer = std::make_unique<OldRenderer>( m_main_wnd, m_main_width, m_main_height );
    m_renderer->Init();

	auto& scene = m_renderer->GetScene();

    TextureID skybox_tex = scene.LoadStaticTexture( "D:/scenes/bistro/green_point_park_4k.DDS" );
    TransformID skybox_tf = scene.AddTransform();
    CubemapID skybox_cubemap = scene.AddCubemapFromTexture( skybox_tex );
    m_skybox = scene.AddEnviromentMap( skybox_cubemap, skybox_tf );

    Camera::Data camera_data;
    camera_data.type = Camera::Type::Perspective;
	camera_data.aspect_ratio = float( m_main_width ) / float( m_main_height );
	camera_data.dir = DirectX::XMFLOAT3( 0, 0, 1 );
	camera_data.near_plane = 0.1f;
	camera_data.far_plane = 1000.0f;
	camera_data.fov_y = DirectX::XM_PIDIV4;
	camera_data.pos = DirectX::XMFLOAT3( 0, 0, 0 );
	camera_data.up = DirectX::XMFLOAT3( 0, 1, 0 );
    m_main_camera = scene.AddCamera( camera_data );
    m_renderer->SetMainCamera( m_main_camera );
    scene.ModifyCamera( m_main_camera )->SetSkybox() = m_skybox;

    return true;
}


LRESULT Application::MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch ( msg )
    {
        // WM_ACTIVATE is sent when the window is activated or deactivated.  
        // We pause the game when the window is deactivated and unpause it 
        // when it becomes active.  
        case WM_ACTIVATE:
            if ( LOWORD( wParam ) == WA_INACTIVE )
            {
                m_paused = true;
                m_timer.Stop();
            }
            else
            {
                m_paused = false;
                m_timer.Start();
            }
            return 0;

            // WM_SIZE is sent when the user resizes the window.  
        case WM_SIZE:
            // Save the new client area dimensions.
            m_main_width = LOWORD( lParam );
            m_main_height = HIWORD( lParam );

            if ( wParam == SIZE_MINIMIZED )
            {
                m_paused = true;
                m_minimized = true;
                m_maximized = false;
            }
            else if ( wParam == SIZE_MAXIMIZED )
            {
                m_paused = false;
                m_minimized = false;
                m_maximized = true;
                OnResize();
            }
            else if ( wParam == SIZE_RESTORED )
            {

                // Restoring from minimized state?
                if ( m_minimized )
                {
                    m_paused = false;
                    m_minimized = false;
                    OnResize();
                }

                // Restoring from maximized state?
                else if ( m_maximized )
                {
                    m_paused = false;
                    m_maximized = false;
                    OnResize();
                }
                else if ( m_resizing )
                {
                    // If user is dragging the resize bars, we do not resize 
                    // the buffers here because as the user continuously 
                    // drags the resize bars, a stream of WM_SIZE messages are
                    // sent to the window, and it would be pointless (and slow)
                    // to resize for each WM_SIZE message received from dragging
                    // the resize bars.  So instead, we reset after the user is 
                    // done resizing the window and releases the resize bars, which 
                    // sends a WM_EXITSIZEMOVE message.
                }
                else
                {
                    OnResize();
                }
            }
            return 0;

        // WM_ENTERSIZEMOVE is sent when the user grabs the resize bars.
        case WM_ENTERSIZEMOVE:
            m_paused = true;
            m_resizing = true;
            m_timer.Stop();
            return 0;

        // WM_EXITSIZEMOVE is sent when the user releases the resize bars.
        // Here we reset everything based on the new window dimensions.
        case WM_EXITSIZEMOVE:
            m_paused = false;
            m_resizing = false;
            m_timer.Start();
            OnResize();
            return 0;

        // WM_DESTROY is sent when the window is being destroyed.
        case WM_DESTROY:
            PostQuitMessage( 0 );
            return 0;

        // The WM_MENUCHAR message is sent when a menu is active and the user presses 
        // a key that does not correspond to any mnemonic or accelerator key. 
        case WM_MENUCHAR:
            // Don't beep when we alt-enter.
            return MAKELRESULT( 0, MNC_CLOSE );

        // Catch this message so to prevent the window from becoming too small.
        case WM_GETMINMAXINFO:
            ( (MINMAXINFO*)lParam )->ptMinTrackSize.x = 200;
            ( (MINMAXINFO*)lParam )->ptMinTrackSize.y = 200;
            return 0;

        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
            OnMouseDown( wParam, GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) );
            return 0;
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
            OnMouseUp( wParam, GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) );
            return 0;
        case WM_MOUSEMOVE:
            OnMouseMove( wParam, GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) );
            return 0;
    }

    return DefWindowProc( hwnd, msg, wParam, lParam );
}


int Application::Run()
{
    MSG msg = { 0 };

    m_timer.Reset();

    while ( msg.message != WM_QUIT )
    {
        // If there are Window messages then process them.
        if ( PeekMessage( &msg, 0, 0, 0, PM_REMOVE ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
        // Otherwise, do animation/game stuff.
        else
        {
            m_timer.Tick();

            if ( ! m_paused )
            {
                OPTICK_FRAME( "MainThread" );
                UpdateFrameStats( m_timer, m_frame_stats );
                Update( m_timer );
                Draw( m_timer );
            }
            else
            {
                Sleep( 100 );
            }
        }
    }

    return (int)msg.wParam;
}


void Application::UpdateFrameStats( const GameTimer& timer, FrameStats& stats )
{
    // Code computes the average frames per second, and also the 
    // average time it takes to render one frame.  These stats 
    // are appended to the window caption bar.

    stats.frame_cnt++;

    // Compute averages over one second period.
    if ( ( timer.TotalTime() - stats.time_elapsed ) >= 1.0f )
    {
        double fps = (double)stats.frame_cnt; // fps = frameCnt / 1
        double mspf = 1000.0f / fps;

        std::wstring fps_str = std::to_wstring( fps );
        std::wstring mspf_str = std::to_wstring( mspf );

        std::wstring window_text = L"Geom Debugger";
		window_text +=
            L"    fps: " + fps_str +
            L"   mspf: " + mspf_str;

        SetWindowText( m_main_wnd, window_text.c_str() );

        // Reset for next average.
        stats.frame_cnt = 0;
        stats.time_elapsed += 1.0f;
    }
}


void Application::OnResize()
{
    if ( m_renderer )
	{
        m_renderer->Resize( m_main_width, m_main_height );
		if ( auto* camera = m_renderer->GetScene().ModifyCamera( m_main_camera ) )
			camera->ModifyData().aspect_ratio = float( m_main_width) / float( m_main_height );
	}
}


void Application::Draw( const GameTimer& timer )
{
	if ( ! m_renderer )
		return;

	m_renderer->NewGUIFrame();

    ImGui::NewFrame();
    ImGui::Render();
	m_renderer->Draw( );
}
