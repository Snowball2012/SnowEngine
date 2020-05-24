#include "stdafx.h"

#include "Application.h"

#include <engine/GeomGeneration.h>

#include <optick.h>
#include <imgui/imgui.h>

#include <WindowsX.h>

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
				if ( !SE_ENSURE( createstruct != nullptr ) )
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

	if ( !InitSceneAssets( m_renderer->GetScene(), m_assets ) )
		return false;
	
	if ( !InitSceneObjects( m_assets, m_renderer->GetScene(), m_sceneobjects, m_camera_controller ) )
		return false;

    m_renderer->SetMainCamera( m_sceneobjects.main_camera );

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
        case WM_MOUSEWHEEL:
            OnMouseWheel( GET_WHEEL_DELTA_WPARAM(wParam), GET_KEYSTATE_WPARAM(wParam), GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) );
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
		if ( auto* camera = m_renderer->GetScene().GetWorld().GetComponent<Camera>( m_sceneobjects.main_camera ) )
			camera->ModifyData().aspect_ratio = float( m_main_width) / float( m_main_height );
	}
}


void Application::Update( const GameTimer& timer )
{
	if ( ! SE_ENSURE( m_renderer ) )
        throw SnowEngineException( "no renderer" );

	auto& scene = m_renderer->GetScene();
	Camera* cam_ptr = scene.GetWorld().GetComponent<Camera>( m_sceneobjects.main_camera );
    if ( ! cam_ptr )
        throw SnowEngineException( "no main camera" );

	if ( m_orbit_change_request && ! m_orbit_mode )
	{
		// create a ray from camera and trace it into the scene
		DirectX::XMVECTOR camera_ray_local = DirectX::XMVectorZero();
		const float near_plane_halfheight = std::tan( cam_ptr->GetData().fov_y / 2.0f ) * cam_ptr->GetData().near_plane;
		const float near_plane_halfwidth = near_plane_halfheight * cam_ptr->GetData().aspect_ratio;
		camera_ray_local.m128_f32[0] = ( m_orbit_change_request->first * 2.0f - 1.0f ) * near_plane_halfwidth;
		camera_ray_local.m128_f32[1] = ( 1.0f - m_orbit_change_request->second * 2.0f ) * near_plane_halfheight;
		camera_ray_local.m128_f32[2] = cam_ptr->GetData().near_plane;

		DirectX::XMMATRIX cam_tf = DirectX::XMMatrixLookToLH(
			DirectX::XMLoadFloat3( &cam_ptr->GetData().pos ),
			DirectX::XMLoadFloat3( &cam_ptr->GetData().dir ),
			DirectX::XMLoadFloat3( &cam_ptr->GetData().up ) ); // transpose?

		cam_tf = DirectX::XMMatrixTranspose( cam_tf );

		DirectX::XMVECTOR camera_ray_dir_ws = DirectX::XMVector4Transform( camera_ray_local, cam_tf );
		DirectX::XMVECTOR camera_ray_pos_ws = DirectX::XMLoadFloat3( &cam_ptr->GetData().pos );
		camera_ray_pos_ws.m128_f32[3] = 1.0f;

		float closest_dist = FLT_MAX;
		bool pt_found = false;
		for ( const auto& [id, transform, mesh_with_material] : m_renderer->GetScene().GetWorld().CreateView<Transform, DrawableMesh>() )
		{
			if ( !mesh_with_material.show )
				continue;

			DirectX::XMVECTOR det;
			DirectX::XMMATRIX world2local = DirectX::XMMatrixInverse( &det, transform.local2world );
			DirectX::XMVECTOR camera_pos_local = DirectX::XMVector4Transform( camera_ray_pos_ws, world2local );
			DirectX::XMVECTOR camera_dir_local = DirectX::XMVector4Transform( camera_ray_dir_ws, world2local );

			const StaticSubmesh* submesh = scene.GetROScene().AllStaticSubmeshes().try_get( mesh_with_material.mesh ); 
			if ( ! submesh )
				continue;

			float ray_to_box_dist;
			if ( ! submesh->Box().Intersects(camera_pos_local, DirectX::XMVector3Normalize( camera_dir_local ), ray_to_box_dist ) )
				continue;

			const StaticMesh* mesh = scene.GetROScene().AllStaticMeshes().try_get( submesh->GetMesh() );
			if ( ! mesh )
				continue;

			const auto& vertices = mesh->Vertices();
			const auto& indices = mesh->Indices();

			for ( size_t i = 0; i < indices.size(); i += 3 )
			{
				auto intersection_res = IntersectRayTriangle(
					&vertices[indices[i]].pos.x,  &vertices[indices[i+1]].pos.x,  &vertices[indices[i+2]].pos.x,
					camera_pos_local.m128_f32, camera_dir_local.m128_f32);

				if ( intersection_res.HitDetected( std::sqrt(FLT_EPSILON), FLT_EPSILON, FLT_EPSILON ) )
				{
					const float distance = intersection_res.coords.m128_f32[2];
					if ( closest_dist > distance )
					{
						closest_dist = distance;
						pt_found = true;
					}
				}
			}
		}

		if ( pt_found )
		{
			DirectX::XMVECTOR orbit_pt = DirectX::XMVectorMultiplyAdd(
				camera_ray_dir_ws, DirectX::XMVectorReplicate( closest_dist ),
				camera_ray_pos_ws );
			m_camera_controller.SetOrbitPoint( orbit_pt.m128_f32 );
			m_orbit_mode = true;
		}

	}
	else if ( m_orbit_mode )
	{
		m_camera_controller.Rotate( m_pending_rotation[0], m_pending_rotation[1] );
		m_camera_controller.Zoom( m_pending_zoom );
	}
	
	m_orbit_change_request.reset();
	m_pending_zoom = 0;
	m_pending_rotation[0] = 0;
	m_pending_rotation[1] = 0;

    Camera::Data& cam_data = cam_ptr->ModifyData();
	DirectX::XMStoreFloat3( &cam_data.pos, m_camera_controller.GetPosition() );
	DirectX::XMStoreFloat3( &cam_data.dir, m_camera_controller.GetEyeDir() );
	DirectX::XMStoreFloat3( &cam_data.up, m_camera_controller.GetUp() );
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


void Application::OnMouseDown(WPARAM btnState, int x, int y)
{
	if ( ( btnState & MK_LBUTTON ) && !m_orbit_mode )
	{
		m_orbit_change_request = std::make_pair(
			float( x ) / float( m_main_width ),
			float( y ) / float( m_main_height ) );

		m_last_mouse_pos[0] = x;
		m_last_mouse_pos[1] = y;

	}
	::SetCapture( m_main_wnd );
}


void Application::OnMouseUp(WPARAM btnState, int x, int y)
{
	m_orbit_mode = false;
	::ReleaseCapture();
}


void Application::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ( ( btnState & MK_LBUTTON ) && m_orbit_mode )
	{
		m_pending_rotation[0] -= float( x - m_last_mouse_pos[0] );
		m_pending_rotation[1] -= float( y - m_last_mouse_pos[1] );
		m_last_mouse_pos[0] = x;
		m_last_mouse_pos[1] = y;
	}
}

void Application::OnMouseWheel( int scroll_amount, WPARAM btnState, int x, int y )
{
	if ( m_orbit_mode )
    {
        m_pending_zoom -= float( scroll_amount ) / float( 120 );
    }
}


bool Application::InitSceneAssets( SceneClientView& scene, SceneAssets& assets ) const
{
	// Sky
    TextureID skybox_tex = scene.LoadStaticTexture( "D:/scenes/bistro/green_point_park_4k.DDS" );
	if ( !skybox_tex.valid() )
		return false;

    TransformID skybox_tf = scene.AddTransform();
	if ( !skybox_tf.valid() )
		return false;

    CubemapID skybox_cubemap = scene.AddCubemapFromTexture( skybox_tex );
	if ( !skybox_cubemap.valid() )
		return false;

    assets.skybox = scene.AddEnviromentMap( skybox_cubemap, skybox_tf );
	if ( !assets.skybox.valid() )
		return false;
	scene.ModifyEnviromentMap( assets.skybox )->SetRadianceFactor() = 18.0f;

	// Environment probes
    TextureID irradiance_map_tex = scene.LoadStaticTexture( "D:/scenes/bistro/ibl/irradiance.DDS" );
    m_renderer->SetIrradianceMap( scene.AddCubemapFromTexture( irradiance_map_tex ) );
    m_renderer->SetReflectionProbe( scene.LoadCubemap( "D:/scenes/bistro/ibl/reflection_probe_cm.dds" ) );

	// Material
	TextureID gray_tex = scene.LoadStaticTexture( "resources/textures/gray.dds" );
	TextureID preintegrated_brdf = scene.LoadStaticTexture( "resources/textures/brdf_lut.dds" );
	TextureID default_normal = scene.LoadStaticTexture( "resources/textures/default_deriv_normal.dds" );
	if ( !gray_tex.valid() || !preintegrated_brdf.valid() || !default_normal.valid() )
		return false;

	MaterialPBR::TextureIds mat_textures;
	{
		mat_textures.base_color = gray_tex;
		mat_textures.specular = gray_tex;
		mat_textures.normal = default_normal;
		mat_textures.preintegrated_brdf = preintegrated_brdf;
	}
	assets.default_material = scene.AddMaterial(
		mat_textures,
		DirectX::XMFLOAT3( 0.04f, 0.04f, 0.04f ), // fresnel
		DirectX::XMFLOAT3( 1, 1, 1 )); // albedo

	if ( !assets.default_material.valid() )
		return false;

	// Meshes
	std::vector<Vertex> cube_vertices;
    std::vector<uint32_t> cube_indices;
    {
        cube_vertices.reserve( GeomGeneration::CubeVertices.size() );
        cube_vertices.assign( GeomGeneration::CubeVertices.cbegin(), GeomGeneration::CubeVertices.cend() );
        cube_indices.reserve( GeomGeneration::CubeIndices.size() );
        cube_indices.assign( GeomGeneration::CubeIndices.cbegin(), GeomGeneration::CubeIndices.cend() );
    }
	assets.cube = scene.LoadStaticMesh( "cube", cube_vertices, cube_indices );
	StaticSubmesh::Data submesh_data;
	{
		submesh_data.base_vertex_loc = 0;
		submesh_data.idx_cnt = uint32_t( cube_indices.size() );
		submesh_data.start_index_loc = 0;
	}
	assets.cube_submesh = scene.AddSubmesh( assets.cube, submesh_data );
	if ( !assets.cube.valid() || !assets.cube_submesh.valid() )
		return false;
	
	return true;
}


bool Application::InitSceneObjects( const SceneAssets& assets, SceneClientView& scene, SceneObjects& objects, OrbitCameraController& camera_controller ) const
{
	// Camera
    Camera::Data camera_data;
    camera_data.type = Camera::Type::Perspective;
	camera_data.aspect_ratio = float( m_main_width ) / float( m_main_height );
	camera_data.dir = DirectX::XMFLOAT3( 0, 0, 1 );
	camera_data.near_plane = 0.1f;
	camera_data.far_plane = 1000.0f;
	camera_data.fov_y = DirectX::XM_PIDIV4;
	camera_data.pos = DirectX::XMFLOAT3( 0, 0, -5 );
	camera_data.up = DirectX::XMFLOAT3( 0, 1, 0 );

	camera_controller.SetAngleSpeed( 1.e-2f );
	camera_controller.SetZoomSpeed( 1.2f );
	camera_controller.SetEye( &camera_data.dir.x );
	camera_controller.SetPosition( &camera_data.pos.x );
	camera_controller.SetUp( &camera_data.up.x );

	objects.main_camera = scene.GetWorld().CreateEntity();
	Camera& cam_component = scene.GetWorld().AddComponent<Camera>( objects.main_camera );
	cam_component.ModifyData() = camera_data;
    cam_component.SetSkybox() = assets.skybox;
	
    auto& world = scene.GetWorld();
    objects.demo_cube = world.CreateEntity();
    world.AddComponent<Transform>( objects.demo_cube, Transform{ DirectX::XMMatrixIdentity() } );
    world.AddComponent<DrawableMesh>( objects.demo_cube, DrawableMesh{ assets.cube_submesh, assets.default_material } );

	return true;
}
