#include "RenderApp.h"
#include "stdafx.h"

#include "RenderApp.h"

#include "GeomGeneration.h"

#include "ForwardLightingPass.h"
#include "DepthOnlyPass.h"
#include "TemporalBlendPass.h"

#include <dxtk12/DDSTextureLoader.h>
#include <dxtk12/DirectXHelpers.h>

#include <d3d12sdklayers.h>

#include <future>

using namespace DirectX;

RenderApp::RenderApp( HINSTANCE hinstance, LPSTR cmd_line )
	: D3DApp( hinstance ), m_cmd_line( cmd_line )
{
	mMainWndCaption = L"Snow Engine";
	mClientWidth = 1920;
	mClientHeight = 1080;
}

RenderApp::~RenderApp() = default;

bool RenderApp::Initialize()
{
	if ( ! D3DApp::Initialize() )
		return false;

	m_renderer = std::make_unique<Renderer>( mhMainWnd, mClientWidth, mClientHeight );
	m_renderer->InitD3D();

	if ( strlen( m_cmd_line ) != 0 )
		LoadScene( m_cmd_line );

	InitScene();

	m_keyboard = std::make_unique<DirectX::Keyboard>();
	
	ReleaseIntermediateSceneMemory();

	return true;
}

void RenderApp::OnResize()
{
	if ( m_renderer )
		m_renderer->Resize( mClientWidth, mClientHeight );
}

void RenderApp::Update( const GameTimer& gt )
{
	ReadKeyboardState( gt );

	UpdateGUI();
	UpdateCamera();
	UpdateLights();
}

void RenderApp::UpdateGUI()
{
	m_renderer->NewGUIFrame();

	ImGui::NewFrame();
	{
		ImGui::Begin( "Scene info", nullptr );
		ImGui::PushItemWidth( 150 );
		ImGui::InputFloat3( "Camera position", &m_camera_pos.x, "%.2f" );
		ImGui::NewLine();
		ImGui::SliderFloat( "Camera speed", &m_camera_speed, 1.0f, 100.0f, "%.2f" );
		m_camera_speed = MathHelper::Clamp( m_camera_speed, 1.0f, 100.0f );

		ImGui::NewLine();
		ImGui::Text( "Camera Euler angles:\n\tphi: %.3f\n\ttheta: %.3f", m_phi, m_theta );
		ImGui::NewLine();
		ImGui::Text( "Sun Euler angles:\n\tphi: %.3f\n\ttheta: %.3f", m_sun_phi, m_sun_theta );
		ImGui::NewLine();
		ImGui::InputFloat( "Sun illuminance in lux", &m_sun_illuminance, 0, 0, "%.3f" );
		ImGui::NewLine();
		ImGui::ColorEdit3( "Sun color", (float*)&m_sun_color_corrected );
		ImGui::End();
	}

	{
		ImGui::Begin( "Render settings", nullptr );
		ImGui::Checkbox( "Wireframe mode", &m_wireframe_mode );
		ImGui::NewLine();
		{
			ImGui::BeginChild( "Tonemap settings" );

			ImGui::PushItemWidth( 150 );
			ImGui::SliderFloat( "Max luminance", &m_renderer->m_tonemap_settings.max_luminance, 0.f, 100000.0f, "%.2f" );
			ImGui::SliderFloat( "Min luminance", &m_renderer->m_tonemap_settings.min_luminance, 0.f, 100000.0f, "%.2f" );
			ImGui::Checkbox( "Blur ref luminance 3x3", &m_renderer->m_tonemap_settings.blend_luminance );
			ImGui::EndChild();
		}

		{
			ImGui::Begin( "HBAO settings" );
			ImGui::SliderFloat( "Max radius", &m_renderer->m_hbao_settings.max_r, 0.f, 5.f, "%.2f" );
			float angle_bias_in_deg = m_renderer->m_hbao_settings.angle_bias * 180.0f / DirectX::XM_PI;
			ImGui::SliderFloat( "Angle bias in degrees", &angle_bias_in_deg, 0.f, 90.0f, "%.2f" );
			m_renderer->m_hbao_settings.angle_bias = angle_bias_in_deg / 180.0f * DirectX::XM_PI;
			ImGui::SliderInt( "Number of samples per direction", &m_renderer->m_hbao_settings.nsamples_per_direction, 0, 20 );
			ImGui::End();
		}

		ImGui::End();
	}
	ImGui::Render();
}

void RenderApp::UpdateCamera()
{
	Camera* cam_ptr = m_renderer->GetSceneView().ModifyCamera( m_camera );
	if ( ! cam_ptr )
		throw SnowEngineException( "no main camera" );

	Camera::Data& cam_data = cam_ptr->ModifyData();

	cam_data.dir = SphericalToCartesian( -1.0f, m_phi, m_theta );
	cam_data.pos = m_camera_pos;
	cam_data.up = XMFLOAT3( 0.0f, 1.0f, 0.0f );
	cam_data.fov_y = MathHelper::Pi / 4;
	cam_data.aspect_ratio = AspectRatio();
	cam_data.far_plane = 100.0f;
	cam_data.near_plane = 0.1f;
}

namespace
{
	// wt/m^2
	DirectX::XMFLOAT3 getLightIrradiance( float illuminance_lux, DirectX::XMFLOAT3 color, float gamma )
	{
		// convert to linear rgb
		color.x = std::pow( color.x, gamma );
		color.y = std::pow( color.y, gamma );
		color.z = std::pow( color.z, gamma );


		if ( color.x == 0 && color.y == 0 && color.z == 0 )
			return color; // avoid division by zero

		// 683.0f * (0.2973f * radiance.r + 1.0f * radiance.g + 0.1010f * radiance.b) == illuminance_lux
		// therefore  x * 683.0f * (0.2973f * color.r + 1.0f * color.g + 0.1010f * color.b)  == illuminance_lux, radiance = linear_color * x;
		float x = illuminance_lux / ( 683.0f * ( 0.2973f * color.x + 1.0f * color.y + 0.1010f * color.z ) );

		return color * x;
	}
}

void RenderApp::UpdateLights()
{
	SceneLight* light_ptr = m_renderer->GetSceneView().ModifyLight( m_sun );
	if ( ! light_ptr )
		throw SnowEngineException( "no sun" );

	SceneLight::Data& sun_data = light_ptr->ModifyData();
	{
		sun_data.dir = SphericalToCartesian( -1, m_sun_phi, m_sun_theta );
		sun_data.strength = getLightIrradiance( m_sun_illuminance, m_sun_color_corrected, 2.2f );
	}
	SceneLight::Shadow sun_shadow;
	{
		sun_shadow.most_detailed_cascade_ws_halfwidth = 50.0f;
		sun_shadow.orthogonal_ws_height = 100.0f;
		sun_shadow.sm_size = 4096;
	}
	light_ptr->ModifyShadow() = sun_shadow;
}

void RenderApp::ReadEventKeys()
{
	auto kb_state = m_keyboard->GetState();
	if ( kb_state.F3 )
		m_wireframe_mode = !m_wireframe_mode;
}

void RenderApp::ReadKeyboardState( const GameTimer& gt )
{
	auto kb_state = m_keyboard->GetState();

	constexpr float angle_per_second = XM_PIDIV2;

	float angle_step = angle_per_second * gt.DeltaTime();

	if ( ! ImGui::GetIO().WantCaptureKeyboard )
	{
		if ( kb_state.Left )
			m_sun_theta -= angle_step;
		if ( kb_state.Right )
			m_sun_theta += angle_step;
		if ( kb_state.Up )
			m_sun_phi += angle_step;
		if ( kb_state.Down )
			m_sun_phi -= angle_step;
		if ( kb_state.W )
			m_camera_pos += SphericalToCartesian( -m_camera_speed * gt.DeltaTime(), m_phi, m_theta );
		if ( kb_state.S )
			m_camera_pos += SphericalToCartesian( m_camera_speed * gt.DeltaTime(), m_phi, m_theta );
	}
	m_sun_phi = boost::algorithm::clamp( m_sun_phi, 0, DirectX::XM_PI );
	m_sun_theta = fmod( m_sun_theta, DirectX::XM_2PI );
}

void RenderApp::Draw( const GameTimer& gt )
{
	Renderer::Context ctx;
	{
		ctx.taa_enabled = m_taa_enabled;
		ctx.wireframe_mode = m_wireframe_mode;
	}
	m_renderer->Draw( ctx );
}

void RenderApp::OnMouseDown( WPARAM btn_state, int x, int y )
{
	if ( ! ImGui::GetIO().WantCaptureMouse )
	{
		m_last_mouse_pos.x = x;
		m_last_mouse_pos.y = y;

		SetCapture( mhMainWnd );
	}
}

void RenderApp::OnMouseUp( WPARAM btn_state, int x, int y )
{
	if ( ! ImGui::GetIO().WantCaptureMouse )
	{
		ReleaseCapture();
	}
}

void RenderApp::OnMouseMove( WPARAM btnState, int x, int y )
{
	if ( ! ImGui::GetIO().WantCaptureMouse )
	{
		if ( ( btnState & MK_LBUTTON ) != 0 )
		{
			// Make each pixel correspond to a quarter of a degree.
			float dx = XMConvertToRadians( 0.25f*static_cast<float>( x - m_last_mouse_pos.x ) );
			float dy = XMConvertToRadians( 0.25f*static_cast<float>( y - m_last_mouse_pos.y ) );

			// Update angles based on input to orbit camera around box.
			m_theta -= dx;
			m_phi -= dy;

			// Restrict the angle phi.
			m_phi = MathHelper::Clamp( m_phi, 0.1f, MathHelper::Pi - 0.1f );
		}
		else if ( ( btnState & MK_RBUTTON ) != 0 )
		{
			// Make each pixel correspond to 0.005 unit in the scene.
			float dx = 0.005f*static_cast<float>( x - m_last_mouse_pos.x );
			float dy = 0.005f*static_cast<float>( y - m_last_mouse_pos.y );

			// Update the camera radius based on input.
			m_camera_speed += ( dx - dy ) * m_camera_speed;

			// Restrict the radius.
			m_camera_speed = MathHelper::Clamp( m_camera_speed, 1.0f, 100.0f );
		}

		m_last_mouse_pos.x = x;
		m_last_mouse_pos.y = y;
	}
}

extern LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

LRESULT RenderApp::MsgProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	if ( ImGui_ImplWin32_WndProcHandler( hwnd, msg, wParam, lParam ) )
		return 1;

	switch ( msg )
	{
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYUP:
			Keyboard::ProcessMessage( msg, wParam, lParam );
			if ( ! ImGui::GetIO().WantCaptureKeyboard )
			{
				ReadEventKeys();
			}
			break;
	}

	return D3DApp::MsgProc( hwnd, msg, wParam, lParam );
}

void RenderApp::LoadScene( const std::string& filename )
{
	LoadFbxFromFile( filename, m_imported_scene );
}

void RenderApp::InitScene()
{
	m_renderer->Init( m_imported_scene );

	Camera::Data camera_data;
	camera_data.type = Camera::Type::Perspective;
	m_camera = m_renderer->GetSceneView().AddCamera( camera_data );
	m_renderer->SetMainCamera( m_camera );
	SceneLight::Data sun_data;
	sun_data.type = SceneLight::LightType::Parallel;
	m_sun = m_renderer->GetSceneView().AddLight( sun_data );
}

void RenderApp::ReleaseIntermediateSceneMemory()
{
	m_imported_scene.indices.clear();
	m_imported_scene.materials.clear();
	m_imported_scene.submeshes.clear();
	m_imported_scene.textures.clear();
	m_imported_scene.vertices.clear();

	m_imported_scene.indices.shrink_to_fit();
	m_imported_scene.materials.shrink_to_fit();
	m_imported_scene.submeshes.shrink_to_fit();
	m_imported_scene.textures.shrink_to_fit();
	m_imported_scene.vertices.shrink_to_fit();
}

XMMATRIX RenderApp::CalcProjectionMatrix() const
{
	return XMMatrixPerspectiveFovLH( MathHelper::Pi / 4, AspectRatio(), 0.1f, 100.0f );
}
