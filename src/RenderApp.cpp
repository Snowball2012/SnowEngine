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
		LoadModel( m_cmd_line );

	m_renderer->Init( m_imported_scene );

	XMMATRIX proj = CalcProjectionMatrix();
	XMStoreFloat4x4( &m_renderer->GetScene().proj, proj );

	m_keyboard = std::make_unique<DirectX::Keyboard>();
	
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

	return true;
}

void RenderApp::OnResize()
{
	if ( m_renderer )
	{
		m_renderer->Resize( mClientWidth, mClientHeight );

		// Need to recompute projection matrix
		XMMATRIX proj = CalcProjectionMatrix();
		XMStoreFloat4x4( &m_renderer->GetScene().proj, proj );
	}
}

void RenderApp::Update( const GameTimer& gt )
{
	m_renderer->NewGUIFrame();

	auto& taa = m_renderer->TemporalAntiAliasing();
	taa.SetFrame( m_cur_frame_idx );

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
		ImGui::InputFloat( "Sun illuminance in lux", &m_sun_illuminance, 0,0,"%.3f" );
		ImGui::NewLine();
		ImGui::ColorEdit3( "Sun color", (float*)&m_sun_color_corrected );
		ImGui::End();
	}

	{
		ImGui::Begin( "Render settings", nullptr );
		ImGui::Checkbox( "Wireframe mode", &m_wireframe_mode );
		ImGui::NewLine();
		ImGui::Checkbox( "Enable TAA", &m_taa_enabled );
		if ( m_taa_enabled )
		{
			ImGui::BeginChild( "TXAA settings" );

			ImGui::PushItemWidth( 150 );
			ImGui::Checkbox( "Jitter projection matrix", &taa.SetJitter() );
			if ( taa.IsJitterEnabled() )
				ImGui::SliderFloat( "Jitter value (px)", &taa.SetJitterVal(), 0.f, 5.0f, "%.2f" );
			ImGui::Checkbox( "Blend frames", &taa.SetBlend() );
			if ( taa.IsBlendEnabled() )
			{
				ImGui::SliderFloat( "Previous frame blend %", &taa.SetBlendFeedback(), 0.f, 0.99f, "%.2f" );
				ImGui::SliderFloat( "Color window expansion %", &taa.SetColorWindowExpansion(), 0.f, 1.f, "%.2f" );
			}

			ImGui::EndChild();
		}

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

	ReadKeyboardState( gt );

	// Update object constants if needed
	for ( auto& renderitem : m_renderer->GetScene().renderitems )
		UpdateRenderItem( renderitem, *m_renderer->GetCurFrameResources().object_cb );

	// Update pass constants
	UpdatePassConstants( gt, *m_renderer->GetCurFrameResources().pass_cb );
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

void RenderApp::UpdatePassConstants( const GameTimer& gt, Utils::UploadBuffer<PassConstants>& pass_cb )
{
	auto& scene = m_renderer->GetScene();
	auto& taa = m_renderer->TemporalAntiAliasing();

	auto cartesian_target = SphericalToCartesian( -1.0f, m_phi, m_theta );

	cartesian_target += m_camera_pos;
	XMVECTOR target = XMLoadFloat3( &cartesian_target );
	XMVECTOR up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

	XMMATRIX view = XMMatrixLookAtLH( XMLoadFloat3( &m_camera_pos ), target, up );
	XMStoreFloat4x4( &scene.view, XMMatrixTranspose( view ) );


	XMFLOAT4X4 proj_jittered = scene.proj;
	if ( m_taa_enabled && taa.IsJitterEnabled() )
		proj_jittered = taa.JitterProjection( proj_jittered, mClientWidth, mClientHeight );
	XMMATRIX proj = XMLoadFloat4x4( &proj_jittered );

	const auto& vp = view * proj;

	PassConstants pc;
	XMStoreFloat4x4( &pc.ViewProj, XMMatrixTranspose( vp ) );
	pc.Proj = scene.proj;
	auto det = XMMatrixDeterminant( XMLoadFloat4x4( &scene.proj ) );
	XMStoreFloat4x4( &pc.InvProj, XMMatrixTranspose( XMMatrixInverse( &det, XMLoadFloat4x4( &scene.proj ) ) ) );

	pc.View = scene.view;
	pc.EyePosW = m_camera_pos;

	pc.FarZ = 100.0f;
	pc.FovY = MathHelper::Pi / 4;
	pc.AspectRatio = AspectRatio();

	pc.use_linear_depth = 1;

	UpdateLights( pc );

	pass_cb.CopyData( 0, pc );

	// shadow map pass constants
	{
		pc.ViewProj = scene.lights["sun"].data.shadow_map_matrix;
		pc.use_linear_depth = 0;
		pc.FarZ = 1000.0f;
		pass_cb.CopyData( 1, pc );
	}

	// main pass
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

void RenderApp::UpdateLights( PassConstants& pc )
{
	auto& scene = m_renderer->GetScene();

	// update sun dir
	auto& sun_light = scene.lights["sun"];
	{
		sun_light.data.dir = SphericalToCartesian( -1, m_sun_phi, m_sun_theta );

		const auto& cartesian = SphericalToCartesian( -100, m_sun_phi, m_sun_theta );

		XMVECTOR target = XMLoadFloat3( &m_camera_pos );
		XMVECTOR pos = XMVectorSet( cartesian.x, cartesian.y, cartesian.z, 1.0f ) + target;
		XMVECTOR up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

		XMMATRIX view = XMMatrixLookAtLH( pos, target, up );
		XMMATRIX proj = XMMatrixOrthographicLH( 50, 50, 10.0f, 200 );
		const auto& viewproj = view * proj;
		XMStoreFloat4x4( &sun_light.data.shadow_map_matrix, XMMatrixTranspose( viewproj ) );
		sun_light.data.strength = getLightIrradiance( m_sun_illuminance, m_sun_color_corrected, 2.2f );
	}

	bc::static_vector<const LightConstants*, MAX_LIGHTS> parallel_lights, point_lights, spotlights;
	for ( const auto& light : scene.lights )
	{
		switch ( light.second.type )
		{
			case Light::Type::Parallel:
				parallel_lights.push_back( &light.second.data );
				break;
			case Light::Type::Point:
				point_lights.push_back( &light.second.data );
				break;
			case Light::Type::Spotlight:
				spotlights.push_back( &light.second.data );
				break;
			default:
				throw std::exception( "not implemented" );
		}
	}

	if ( parallel_lights.size() + point_lights.size() + spotlights.size() > MAX_LIGHTS )
		throw std::exception( "too many lights" );

	pc.n_parallel_lights = int( parallel_lights.size() );
	pc.n_point_lights = int( point_lights.size() );
	pc.n_spotlight_lights = int( spotlights.size() );

	size_t light_offset = 0;
	for ( const LightConstants* light : boost::range::join( parallel_lights,
															boost::range::join( point_lights,
																				spotlights ) ) )
	{
		pc.lights[light_offset++] = *light;
	}
}

void RenderApp::UpdateRenderItem( RenderItem& renderitem, Utils::UploadBuffer<ObjectConstants>& obj_cb )
{
	if ( renderitem.n_frames_dirty > 0 )
	{
		ObjectConstants obj_constants;
		XMStoreFloat4x4( &obj_constants.model, XMMatrixTranspose( XMLoadFloat4x4( &renderitem.world_mat ) ) );
		XMStoreFloat4x4( &obj_constants.model_inv_transpose, XMMatrixTranspose( InverseTranspose( XMLoadFloat4x4( &renderitem.world_mat ) ) ) );
		obj_cb.CopyData( renderitem.cb_idx, obj_constants );
		renderitem.n_frames_dirty--;
	}
}


void RenderApp::Draw( const GameTimer& gt )
{
	Renderer::Context ctx;
	{
		ctx.taa_enabled = m_taa_enabled;
		ctx.wireframe_mode = m_wireframe_mode;
	}
	m_renderer->Draw( ctx );
	m_cur_frame_idx++;
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

void RenderApp::LoadModel( const std::string& filename )
{
	LoadFbxFromFile( filename, m_imported_scene );
}

XMMATRIX RenderApp::CalcProjectionMatrix() const
{
	return XMMatrixPerspectiveFovLH( MathHelper::Pi / 4, AspectRatio(), 1.0f, 100.0f );
}
