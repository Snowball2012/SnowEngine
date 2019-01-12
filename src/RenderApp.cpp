// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
	: D3DApp( hinstance ), m_cmd_line( cmd_line ), m_last_mouse_pos{ 0, 0 }
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
	m_renderer->Init();

	if ( strlen( m_cmd_line ) != 0 ) //-V805
		m_is_scene_loaded = std::async( std::launch::async, [this]() { LoadScene( m_cmd_line ); } );

	LoadPlaceholderTextures();

	m_loading_screen.Init( m_renderer->GetSceneView(), m_ph_normal_texture, m_ph_specular_texture );
	m_loading_screen.Enable( m_renderer->GetSceneView(), *m_renderer );

	m_keyboard = std::make_unique<DirectX::Keyboard>();

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

	if ( m_cur_state == State::Loading )
	{
		if ( m_is_scene_loaded.wait_for( std::chrono::seconds( 0 ) ) == std::future_status::ready )
		{
			m_cur_state = State::Main;
			m_loading_screen.Disable( m_renderer->GetSceneView(), *m_renderer );
			InitScene();
		}
	}

	if ( m_cur_state == State::Loading )
	{
		m_loading_screen.Update( m_renderer->GetSceneView(), mClientWidth, mClientHeight, gt );
	}
	else
	{
		UpdateCamera();
		UpdateLights();
	}
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
		float pssm_interpolator = m_renderer->PSSM().GetUniformFactor();
		ImGui::SliderFloat( "INTERPOLATOR", &pssm_interpolator,0 , 1, "%.2f" );
		m_renderer->PSSM().SetUniformFactor( pssm_interpolator );

		ImGui::Checkbox( "Separate camera for frustrum culling", &m_dbg_use_separate_camera );
		if ( m_dbg_use_separate_camera )
		{
			m_renderer->SetMainCamera( m_dbg_frustrum_camera );
			m_renderer->SetFrustrumCullCamera( m_camera );
		}
		else
		{
			m_renderer->SetMainCamera( m_camera );
			m_renderer->SetFrustrumCullCamera( CameraID::nullid );
		}

		ImGui::NewLine();
		{
			ImGui::BeginChild( "Tonemap settings" );

			ImGui::PushItemWidth( 150 );
			ImGui::SliderFloat( "Max luminance", &m_renderer->m_tonemap_settings.max_luminance, 0.f, 100000.0f, "%.2f" ); //-V807
			ImGui::SliderFloat( "Min luminance", &m_renderer->m_tonemap_settings.min_luminance, 0.f, 100000.0f, "%.2f" );
			ImGui::Checkbox( "Blur ref luminance 3x3", &m_renderer->m_tonemap_settings.blend_luminance );
			ImGui::EndChild();
		}

		{
			ImGui::Begin( "HBAO settings" );
			ImGui::SliderFloat( "Max radius", &m_renderer->m_hbao_settings.max_r, 0.f, 5.f, "%.2f" ); //-V807
			float angle_bias_in_deg = m_renderer->m_hbao_settings.angle_bias * 180.0f / DirectX::XM_PI;
			ImGui::SliderFloat( "Angle bias in degrees", &angle_bias_in_deg, 0.f, 90.0f, "%.2f" );
			m_renderer->m_hbao_settings.angle_bias = angle_bias_in_deg / 180.0f * DirectX::XM_PI;
			ImGui::SliderInt( "Number of samples per direction", &m_renderer->m_hbao_settings.nsamples_per_direction, 0, 20 );
			ImGui::End();
		}

		ImGui::End();
	}

	{
		ImGui::Begin( "Performance", nullptr );

		Renderer::PerformanceStats stats = m_renderer->GetPerformanceStats();

		ImGui::Text( "TextureStreamer vidmem:\n\tIn use: %u MB\n\tTotal:  %u MB", stats.tex_streamer.vidmem_in_use / ( 1024 * 1024 ), stats.tex_streamer.vidmem_allocated / (1024*1024) );
		ImGui::NewLine();
		ImGui::Text( "TextureStreamer upload mem:\n\tIn use: %u MB\n\tTotal:  %u MB", stats.tex_streamer.uploader_mem_in_use / ( 1024 * 1024 ), stats.tex_streamer.uploader_mem_allocated / ( 1024 * 1024 ) );
		ImGui::NewLine();


		ImGui::End();
	}
	ImGui::Render();
}

void RenderApp::UpdateCamera()
{
	// main camera
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
		cam_data.far_plane = 500.0f;
		cam_data.near_plane = 0.1f;
	}

	// debug camera
	{
		Camera* cam_ptr = m_renderer->GetSceneView().ModifyCamera( m_dbg_frustrum_camera );
		if ( ! cam_ptr )
			throw SnowEngineException( "no debug camera" );

		Camera::Data& cam_data = cam_ptr->ModifyData();

		cam_data.pos = DirectX::XMFLOAT3( 30.f, 30.f, -30.f );
		cam_data.dir = cam_data.pos * -1.0f;
		XMFloat3Normalize( cam_data.dir );
		cam_data.up = XMFLOAT3( 0.0f, 1.0f, 0.0f );
		cam_data.fov_y = MathHelper::Pi / 4;
		cam_data.aspect_ratio = AspectRatio();
		cam_data.far_plane = 100.0f;
		cam_data.near_plane = 0.1f;
	}
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


		if ( color.x == 0 && color.y == 0 && color.z == 0 ) //-V550
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
		sun_shadow.ws_halfwidth = 50.0f;
		sun_shadow.orthogonal_ws_height = 100.0f;
		sun_shadow.sm_size = 2048;
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
	LoadAndBuildTextures( m_imported_scene, true );
	BuildGeometry( m_imported_scene );
	BuildMaterials( m_imported_scene );
	BuildRenderItems( m_imported_scene );
	ReleaseIntermediateSceneMemory();

	auto& scene = m_renderer->GetSceneView();

	Camera::Data camera_data;
	camera_data.type = Camera::Type::Perspective;
	m_camera = scene.AddCamera( camera_data );
	m_renderer->SetMainCamera( m_camera );
	SceneLight::Data sun_data;
	sun_data.type = SceneLight::LightType::Parallel;
	m_sun = scene.AddLight( sun_data );

	Camera::Data dbg_frustrum_cam_data;
	camera_data.type = Camera::Type::Perspective;
	m_dbg_frustrum_camera = scene.AddCamera( dbg_frustrum_cam_data );
}


void RenderApp::LoadAndBuildTextures( ImportedScene& ext_scene, bool flush_per_texture )
{
	auto& scene = m_renderer->GetSceneView();
	for ( size_t i = 0; i < ext_scene.textures.size(); ++i )
		ext_scene.textures[i].second = scene.LoadStreamedTexture( ext_scene.textures[i].first );
}

void RenderApp::LoadPlaceholderTextures()
{
	m_ph_normal_texture = m_renderer->GetSceneView().LoadStreamedTexture( "resources/textures/default_deriv_normal.dds" );
	m_ph_specular_texture = m_renderer->GetSceneView().LoadStreamedTexture( "resources/textures/default_spec.dds" );
}

void RenderApp::BuildGeometry( ImportedScene& ext_scene )
{
	auto& scene = m_renderer->GetSceneView();

	ext_scene.mesh_id = scene.LoadStaticMesh(
		"main",
		ext_scene.vertices,
		ext_scene.indices );
}

void RenderApp::BuildMaterials( ImportedScene& ext_scene )
{
	auto& scene = m_renderer->GetSceneView();

	for ( int i = 0; i < ext_scene.materials.size(); ++i )
	{
		MaterialPBR::TextureIds textures;
		ImportedScene::SceneMaterial& ext_material = ext_scene.materials[i].second;
		textures.base_color = ext_scene.textures[ext_material.base_color_tex_idx].second;
		textures.normal = ext_scene.textures[ext_material.normal_tex_idx].second;
		int spec_map_idx = ext_material.specular_tex_idx;
		if ( spec_map_idx < 0 )
			textures.specular = m_ph_specular_texture;
		else
			textures.specular = ext_scene.textures[spec_map_idx].second;

		ext_material.material_id = scene.AddMaterial( textures, XMFLOAT3( 0.03f, 0.03f, 0.03f ) );
	}
}

void RenderApp::BuildRenderItems( const ImportedScene& ext_scene )
{
	auto& scene = m_renderer->GetSceneView();

	for ( const auto& ext_submesh : ext_scene.submeshes )
	{
		StaticSubmeshID submesh_id = scene.AddSubmesh( ext_scene.mesh_id,
													   StaticSubmesh::Data{ uint32_t( ext_submesh.nindices ),
																			uint32_t( ext_submesh.index_offset ),
																			0 } );
		RenderItem item;

		const int material_idx = ext_submesh.material_idx;
		if ( material_idx < 0 )
			throw SnowEngineException( "no material!" );

		MaterialID mat = ext_scene.materials[material_idx].second.material_id;
		TransformID tf = scene.AddTransform( ext_submesh.transform );

		MeshInstanceID mesh_instance = scene.AddMeshInstance( submesh_id, tf, mat );
	}
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

void RenderApp::LoadingScreen::Init( SceneClientView& scene, TextureID normal_tex_id, TextureID specular_tex_id )
{
	Camera::Data camera_data;
	camera_data.type = Camera::Type::Perspective;
	camera_data.pos = DirectX::XMFLOAT3( 0, 1.7f, -3 );
	camera_data.dir = DirectX::XMFLOAT3( 0, -0.3f, 1 );
	XMFloat3Normalize( camera_data.dir );
	camera_data.up = DirectX::XMFLOAT3( 0, 1, 0 );
	camera_data.fov_y = MathHelper::Pi / 4;
	camera_data.far_plane = 10000.0f;
	camera_data.near_plane = 0.1f;
	m_camera = scene.AddCamera( camera_data );

	SceneLight::Data light_data;
	light_data.type = SceneLight::LightType::Parallel;
	light_data.dir = DirectX::XMFLOAT3( 1, 1, -1 );
	XMFloat3Normalize( light_data.dir );
	light_data.strength = getLightIrradiance( 70.0e3f, DirectX::XMFLOAT3( 1, 1, 1 ), 2.2f );
	m_light = scene.AddLight( light_data );
	scene.ModifyLight( m_light )->ModifyShadow() = SceneLight::Shadow{ 512, 3.0f, 3.0f };

	LoadCube( scene, normal_tex_id, specular_tex_id );
}

void RenderApp::LoadingScreen::Enable( SceneClientView& scene, Renderer& renderer )
{
	renderer.SetFrustrumCullCamera( CameraID::nullid );
	renderer.SetMainCamera( m_camera );
	SceneLight* light = scene.ModifyLight( m_light );
	if ( ! light )
		throw SnowEngineException( "light not found!" );
	light->IsEnabled() = true;

	StaticMeshInstance* cube = scene.ModifyInstance( m_cube );
	if ( ! cube )
		throw SnowEngineException( "cube not found!" );
	cube->IsEnabled() = true;
}

void RenderApp::LoadingScreen::Disable( SceneClientView& scene, Renderer& renderer )
{
	renderer.SetMainCamera( CameraID::nullid );
	SceneLight* light = scene.ModifyLight( m_light );
	if ( ! light )
		throw SnowEngineException( "light not found!" );
	light->IsEnabled() = false;

	StaticMeshInstance* cube = scene.ModifyInstance( m_cube );
	if ( ! cube )
		throw SnowEngineException( "cube not found!" );
	cube->IsEnabled() = false;
}

void RenderApp::LoadingScreen::Update( SceneClientView& scene, float screen_width, float screen_height, const GameTimer& gt )
{
	Camera* cam = scene.ModifyCamera( m_camera );
	if ( ! cam )
		throw SnowEngineException( "camera not found!" );

	auto& cam_data = cam->ModifyData();
	cam_data.aspect_ratio = screen_width / screen_height;

	StaticMeshInstance* cube = scene.ModifyInstance( m_cube );
	if ( ! cube )
		throw SnowEngineException( "cube not found!" );
	ObjectTransform* tf = scene.ModifyTransform( cube->GetTransform() );
	assert( tf );
	m_theta = gt.TotalTime();
	XMFLOAT3 cube_eye_dir = SphericalToCartesian( 1, MathHelper::Pi / 2.0f, m_theta );
	auto& cube_local2world = tf->ModifyMat();

	auto scale = XMMatrixScaling( 0.1f, 0.1f, 0.1f );
	auto rotation = XMMatrixLookToLH( XMVectorZero(),
									  XMLoadFloat3( &cube_eye_dir ),
									  XMVectorSet( 0, 1, 0, 0 ) );
	auto translation = XMMatrixTranslation( 1.0f * cam_data.aspect_ratio, -0.3f, 0 );
	XMStoreFloat4x4( &cube_local2world, scale * rotation * translation );

	
}

void RenderApp::LoadingScreen::LoadCube( SceneClientView& scene, TextureID normal_tex_id, TextureID specular_tex_id )
{
	TransformID tf_id = scene.AddTransform( Identity4x4 );
	TextureID loading_cube_texture = scene.LoadStreamedTexture( "resources/textures/loading_box_base.dds" );
	MaterialID cube_material_id = scene.AddMaterial( MaterialPBR::TextureIds{ loading_cube_texture, normal_tex_id, specular_tex_id },
													 DirectX::XMFLOAT3( 0.3f, 0.3f, 0.3f ), Identity4x4 );

	std::vector<Vertex> cube_vertices;
	std::vector<uint32_t> cube_indices;
	{
		cube_vertices.reserve( GeomGeneration::CubeVertices.size() );
		cube_vertices.assign( GeomGeneration::CubeVertices.cbegin(), GeomGeneration::CubeVertices.cend() );
		cube_indices.reserve( GeomGeneration::CubeIndices.size() );
		cube_indices.assign( GeomGeneration::CubeIndices.cbegin(), GeomGeneration::CubeIndices.cend() );
	}
	StaticMeshID mesh_id = scene.LoadStaticMesh( "loading cube", std::move( cube_vertices ), std::move( cube_indices ) );
	StaticSubmeshID submesh_id = scene.AddSubmesh( mesh_id, StaticSubmesh::Data{ uint32_t( GeomGeneration::CubeIndices.size() ), 0, 0 } );

	m_cube = scene.AddMeshInstance( submesh_id, tf_id, cube_material_id );
}
