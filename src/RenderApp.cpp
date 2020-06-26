// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <engine/stdafx.h>

#include "RenderApp.h"

#include <engine/GeomGeneration.h>

#include <engine/ForwardLightingPass.h>
#include <engine/DepthOnlyPass.h>
#include <engine/TemporalBlendPass.h>

#include <engine/SceneItems.h>

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

    m_renderer = std::make_unique<OldRenderer>( mhMainWnd, mClientWidth, mClientHeight );
    m_renderer->Init();

    if ( strlen( m_cmd_line ) != 0 ) //-V805
        m_is_scene_loaded = std::async( std::launch::async, [this]() { return LoadScene( m_cmd_line ); } );

    LoadPlaceholderTextures();

    m_loading_screen.Init( m_renderer->GetScene(), m_ph_normal_texture, m_ph_specular_texture, m_ph_skybox );
    m_loading_screen.Enable( m_renderer->GetScene(), *m_renderer );

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
    OPTICK_EVENT();
    ReadKeyboardState( gt );

    UpdateGUI();

    if ( m_cur_state == State::Loading )
    {
        if ( m_is_scene_loaded.wait_for( std::chrono::seconds( 0 ) ) == std::future_status::ready )
        {
            m_cur_state = State::Main;
            m_loading_screen.Disable( m_renderer->GetScene(), *m_renderer );
            InitScene( m_is_scene_loaded.get() );
			InitHighlightCube();
        }
    }

    if ( m_cur_state == State::Loading )
    {
        m_loading_screen.Update( m_renderer->GetScene(), mClientWidth, mClientHeight, gt );
    }
    else
    {
        UpdateCamera();
        UpdateLights();
		UpdateHighlightedObject();
    }
}

void RenderApp::UpdateGUI()
{
    OPTICK_EVENT();
    m_renderer->NewGUIFrame();

    ImGui::NewFrame();
    {
        ImGui::Begin( "Scene info", nullptr );
        ImGui::PushItemWidth( 150 );
        ImGui::InputFloat3( "Camera position", &m_camera_pos.x, "%.2f" );
        ImGui::NewLine();
        ImGui::SliderFloat( "Camera speed", &m_camera_speed, 1.0f, 100.0f, "%.2f" );
        m_camera_speed = boost::algorithm::clamp( m_camera_speed, 1.0f, 100.0f );

        ImGui::NewLine();
        ImGui::Text( "Camera Euler angles:\n\tphi: %.3f\n\ttheta: %.3f", m_phi, m_theta );
        ImGui::NewLine();

        EnvironmentMap* skybox = m_renderer->GetScene().ModifyEnviromentMap( m_ph_skybox );
        if ( skybox )
        {
            ImGui::SliderFloat( "Skybox luminance multiplier", &m_sky_radiance_factor, 0, 1.e2 );
            skybox->SetRadianceFactor() = m_sky_radiance_factor;

            ImGui::SliderFloat( "Skybox phi", &m_sky_phi, 0, DirectX::XM_2PI );

            ObjectTransform* tf = m_renderer->GetScene().ModifyTransform( m_skybox_tf );
            if ( tf )
            {
                DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationY( m_sky_phi );
                DirectX::XMStoreFloat4x4( &tf->ModifyMat(), rotation );
            }

            ImGui::NewLine();
        }

        ImGui::Text( "Sun Euler angles:\n\tphi: %.3f\n\ttheta: %.3f", m_sun_phi, m_sun_theta );
        ImGui::NewLine();

        ImGui::InputFloat( "Sun illuminance in lux", &m_sun_illuminance, 0, 0 );
        ImGui::NewLine();		

        ImGui::ColorEdit3( "Sun color", (float*)&m_sun_color_corrected );
        ImGui::End();
    }

    {
        ImGui::Begin( "Render settings", nullptr );
        float pssm_interpolator = m_renderer->PSSM().GetUniformFactor();
        ImGui::SliderFloat( "PSSM uniform split factor", &pssm_interpolator,0 , 1, "%.2f" );
        m_renderer->PSSM().SetUniformFactor( pssm_interpolator );

		if ( m_dbg_frustum_camera.valid() && m_camera.valid() )
		{
			bool use_separate_camera_old = m_dbg_use_separate_camera;
			ImGui::Checkbox( "Freeze camera for frustum culling (does not work right now)", &m_dbg_use_separate_camera );
			if ( !use_separate_camera_old && m_dbg_use_separate_camera )
			{
				// Freeze camera
				auto* main_cam = m_renderer->GetScene().GetWorld().GetComponent<Camera>( m_camera );
				auto* dbg_cam = m_renderer->GetScene().GetWorld().GetComponent<Camera>( m_dbg_frustum_camera );
				if ( !SE_ENSURE( main_cam ) || !SE_ENSURE( dbg_cam ) )
					dbg_cam->ModifyData() = main_cam->GetData();
			}
		}

		ImGui::NewLine();
		ImGui::Checkbox( "Highlight object", &m_highlight_object );

        ImGui::NewLine();
        {
            ImGui::BeginChild( "Tonemap settings" );

            ImGui::PushItemWidth( 150 );
            ImGui::SliderFloat( "Max luminance", &m_renderer->m_tonemap_settings.max_luminance, 0.f, 20000.0f, "%.2f" ); //-V807
            ImGui::SliderFloat( "Min luminance", &m_renderer->m_tonemap_settings.min_luminance, 0.f, 20000.0f, "%.2f" );
            ImGui::SliderFloat( "Bloom strength", &m_renderer->m_tonemap_settings.bloom_strength, 0.f, 10.0f, "%.2f" );
            ImGui::SliderFloat( "Bloom mip", &m_renderer->m_tonemap_settings.bloom_mip, 0.f, 10.0f, "%.2f" );
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

        OldRenderer::PerformanceStats stats = m_renderer->GetPerformanceStats();

        ImGui::Text( "TextureStreamer vidmem:\n\tIn use: %u MB\n\tTotal:  %u MB", stats.tex_streamer.vidmem_in_use / ( 1024 * 1024 ), stats.tex_streamer.vidmem_allocated / (1024*1024) );
        ImGui::NewLine();
        ImGui::Text( "TextureStreamer upload mem:\n\tIn use: %u MB\n\tTotal:  %u MB", stats.tex_streamer.uploader_mem_in_use / ( 1024 * 1024 ), stats.tex_streamer.uploader_mem_allocated / ( 1024 * 1024 ) );
        ImGui::NewLine();
        ImGui::Text( "Renderitems:\n\tTotal: %u\n\tAfter culling:  %u", stats.num_renderitems_total, stats.num_renderitems_to_draw );
        ImGui::NewLine();


        ImGui::End();
    }

    {
        if ( m_is_console_opened )
        {
            m_console.Draw( "Console", &m_is_console_opened );
            if (m_cur_state != State::Loading)
                m_renderer->SetSkybox( m_console.Skybox );
        }
    }
    ImGui::Render();
}

void RenderApp::UpdateCamera()
{
    // main camera
    {
        Camera* cam_ptr = m_renderer->GetScene().GetWorld().GetComponent<Camera>( m_camera );
        if ( ! cam_ptr )
            throw SnowEngineException( "no main camera" );

        Camera::Data& cam_data = cam_ptr->ModifyData();

        cam_data.dir = SphericalToCartesian( -1.0f, m_phi, m_theta );
        cam_data.pos = m_camera_pos;
        cam_data.up = XMFLOAT3( 0.0f, 1.0f, 0.0f );
        cam_data.fov_y = XM_PIDIV4;
        cam_data.aspect_ratio = AspectRatio();
        cam_data.far_plane = 200.0f;
        cam_data.near_plane = 0.1f;
    }

    // debug camera
    {
        Camera* cam_ptr = m_renderer->GetScene().GetWorld().GetComponent<Camera>( m_dbg_frustum_camera );
        if ( ! cam_ptr )
            throw SnowEngineException( "no debug camera" );

        Camera::Data& cam_data = cam_ptr->ModifyData();

        cam_data.pos = DirectX::XMFLOAT3( 30.f, 30.f, -30.f );
        cam_data.dir = cam_data.pos * -1.0f;
        XMFloat3Normalize( cam_data.dir );
        cam_data.up = XMFLOAT3( 0.0f, 1.0f, 0.0f );
        cam_data.fov_y = XM_PIDIV4;
        cam_data.aspect_ratio = AspectRatio();
        cam_data.far_plane = 100.0f;
        cam_data.near_plane = 0.1f;
    }

	if ( m_dbg_use_separate_camera )
	{
		m_renderer->SetMainCamera( m_camera );
		m_renderer->SetFrustumCullCamera( m_dbg_frustum_camera );
	}
	else
	{
		m_renderer->SetMainCamera( m_camera );
		m_renderer->SetFrustumCullCamera( World::Entity::nullid );
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
    OPTICK_EVENT();
    Light* light_ptr = m_renderer->GetScene().ModifyLight( m_sun );
    if ( ! light_ptr )
        throw SnowEngineException( "no sun" );

    Light::Data& sun_data = light_ptr->ModifyData();
    {
        sun_data.dir = SphericalToCartesian( -1, m_sun_phi, m_sun_theta );
        sun_data.strength = getLightIrradiance( m_sun_illuminance, m_sun_color_corrected, 2.2f );
    }
    Light::Shadow sun_shadow;
    {
        sun_shadow.orthogonal_ws_height = 100.0f;
        sun_shadow.sm_size = 2048;
        sun_shadow.num_cascades = MAX_CASCADE_SIZE;
    }
    light_ptr->ModifyShadow() = sun_shadow;
}

void RenderApp::UpdateHighlightedObject()
{
	DrawableMesh* cube_mesh = nullptr;
	Transform* cube_tf = nullptr;
	if ( m_highlight_cube.valid() )
	{
		cube_mesh = m_renderer->GetScene().GetWorld().GetComponent<DrawableMesh>( m_highlight_cube );
		if ( ! SE_ENSURE( cube_mesh ) )
			throw SnowEngineException( "no drawable on highlight cube" );

		cube_tf = m_renderer->GetScene().GetWorld().GetComponent<Transform>( m_highlight_cube );
		if ( ! SE_ENSURE( cube_tf ) )
			throw SnowEngineException( "no transform on highlight cube" );
	}

	if ( ! m_highlight_object )
	{
		if ( m_highlighted_material.valid() )
		{
			if ( auto* material = m_renderer->GetScene().ModifyMaterial( m_highlighted_material ) )
				material->Modify().albedo_color = m_highlighted_diffuse;
			m_highlighted_material = MaterialID::nullid;
		}
		if ( cube_mesh )
			cube_mesh->show = false;
	}
	else
	{
		OPTICK_EVENT();
		auto& scene = m_renderer->GetScene();
		Camera* cam_ptr = m_renderer->GetScene().GetWorld().GetComponent<Camera>( m_camera );
        if ( ! cam_ptr )
            throw SnowEngineException( "no main camera" );

        const Camera::Data& cam_data = cam_ptr->GetData();

		DirectX::XMVECTOR camera_pos_ws = XMLoadFloat3( &cam_data.pos );
		camera_pos_ws.m128_f32[3] = 1.0f;
		DirectX::XMVECTOR camera_dir_ws = XMLoadFloat3( &cam_data.dir );
		float closest_dist = FLT_MAX;
		MaterialID closest_material = MaterialID::nullid;
		for ( const auto& [id, transform, mesh_with_material] : m_renderer->GetScene().GetWorld().CreateView<Transform, DrawableMesh>() )
		{
			if ( !mesh_with_material.show )
				continue;

			if ( id == m_highlight_cube )
				continue;

			DirectX::XMVECTOR det;
			DirectX::XMMATRIX world2local = DirectX::XMMatrixInverse( &det, transform.local2world );
			DirectX::XMVECTOR camera_pos_local = DirectX::XMVector4Transform( camera_pos_ws, world2local );
			DirectX::XMVECTOR camera_dir_local = DirectX::XMVector4Transform( camera_dir_ws, world2local );

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
						closest_material = mesh_with_material.material;
					}
				}
			}
		}

		if ( closest_material.valid() )
		{
			if ( cube_mesh )
				cube_mesh->show = true;
			if ( cube_tf )
				cube_tf->local2world =
					DirectX::XMMatrixScaling( 0.1f, 0.1f, 0.1f )
					* DirectX::XMMatrixTranslationFromVector(
						DirectX::XMVectorMultiplyAdd( camera_dir_ws, DirectX::XMVectorReplicate( closest_dist ), camera_pos_ws ) );
		}
		else
		{
			if ( cube_mesh )
				cube_mesh->show = false;
		}

		if ( closest_material != m_highlighted_material )
		{
			if ( m_highlighted_material.valid() )
			{
				if ( auto* material = m_renderer->GetScene().ModifyMaterial( m_highlighted_material ) )
					material->Modify().albedo_color = m_highlighted_diffuse;
				m_highlighted_material = MaterialID::nullid;
			}

			m_highlighted_material = closest_material;
			if ( m_highlighted_material.valid() )
			{
				if ( auto* material = m_renderer->GetScene().ModifyMaterial( m_highlighted_material ) )
				{
					m_highlighted_diffuse = material->Modify().albedo_color;
					material->Modify().albedo_color.x = 2.0f;
					material->Modify().albedo_color.y = 0.1f;
					material->Modify().albedo_color.z = 2.0f;
				}
			}
		}		
	}
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
        if ( kb_state.OemTilde && !m_tilda_was_pressed_last_frame )
            m_is_console_opened = !m_is_console_opened;
    }

    m_tilda_was_pressed_last_frame = kb_state.OemTilde;
    m_sun_phi = boost::algorithm::clamp( m_sun_phi, 0, DirectX::XM_PI );
    m_sun_theta = fmod( m_sun_theta, DirectX::XM_2PI );
}

void RenderApp::Draw( const GameTimer& gt )
{
    m_renderer->Draw( );
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
            m_phi = boost::algorithm::clamp( m_phi, 0.1f, XM_PI - 0.1f );
        }
        else if ( ( btnState & MK_RBUTTON ) != 0 )
        {
            float dx = 0.005f*static_cast<float>( x - m_last_mouse_pos.x );
            float dy = 0.005f*static_cast<float>( y - m_last_mouse_pos.y );

            m_camera_speed += ( dx - dy ) * m_camera_speed;

            m_camera_speed = boost::algorithm::clamp( m_camera_speed, 1.0f, 100.0f );
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
            break;
    }

    return D3DApp::MsgProc( hwnd, msg, wParam, lParam );
}

ImportedScene RenderApp::LoadScene( const std::string& filename )
{
    ImportedScene scene;
    LoadFbxFromFile( filename, scene );
    return std::move( scene );
}

void RenderApp::InitScene( ImportedScene imported_scene )
{
    LoadAndBuildTextures( imported_scene, true );
    BuildGeometry( imported_scene );
    BuildMaterials( imported_scene );
    BuildRenderItems( imported_scene );

    auto& scene = m_renderer->GetScene();
    auto& world = scene.GetWorld();

    Camera::Data camera_data;
    camera_data.type = Camera::Type::Perspective;
    m_camera = world.CreateEntity();
	Camera& cam_component = world.AddComponent<Camera>( m_camera );
	cam_component.ModifyData() = camera_data;
    cam_component.SetSkybox() = m_ph_skybox;

    m_renderer->SetMainCamera( m_camera );
    Light::Data sun_data;
    sun_data.type = Light::LightType::Parallel;
    m_sun = scene.AddLight( sun_data );

	m_dbg_frustum_camera = world.CreateEntity();
	Camera& dbg_cam_component = world.AddComponent<Camera>( m_dbg_frustum_camera );
}


bool RenderApp::InitHighlightCube()
{
	if ( !SE_ENSURE( m_renderer ) )
		return false;

	auto& scene = m_renderer->GetScene();
	auto& world = scene.GetWorld();

	
    TextureID loading_cube_texture = scene.LoadStaticTexture( "resources/textures/loading_box_base.dds" );
    MaterialID cube_material_id = scene.AddMaterial( MaterialPBR::TextureIds{ loading_cube_texture, m_ph_normal_texture, m_ph_specular_texture },
                                                     DirectX::XMFLOAT3( 0.3f, 0.3f, 0.3f ), DirectX::XMFLOAT3( 0.5f, 1, 0.5f ), Identity4x4 );

    std::vector<Vertex> cube_vertices;
    std::vector<uint32_t> cube_indices;
    {
        cube_vertices.reserve( GeomGeneration::CubeVertices.size() );
        cube_vertices.assign( GeomGeneration::CubeVertices.cbegin(), GeomGeneration::CubeVertices.cend() );
        cube_indices.reserve( GeomGeneration::CubeIndices.size() );
        cube_indices.assign( GeomGeneration::CubeIndices.cbegin(), GeomGeneration::CubeIndices.cend() );
    }
    StaticMeshID mesh_id = scene.LoadStaticMesh( "highlight_cube", std::move( cube_vertices ), std::move( cube_indices ) );
    StaticSubmeshID submesh_id = scene.AddSubmesh( mesh_id, StaticSubmesh::Data{ uint32_t( GeomGeneration::CubeIndices.size() ), 0, 0 } );

    m_highlight_cube = world.CreateEntity();
    world.AddComponent<Transform>( m_highlight_cube, Transform{ DirectX::XMMatrixIdentity() } );
    auto& drawable = world.AddComponent<DrawableMesh>( m_highlight_cube, DrawableMesh{ submesh_id, cube_material_id } );
	drawable.show = false;

	return true;
}


void RenderApp::LoadAndBuildTextures( ImportedScene& ext_scene, bool flush_per_texture )
{
    auto& scene = m_renderer->GetScene();
    for ( size_t i = 0; i < ext_scene.textures.size(); ++i )
        ext_scene.textures[i].second = scene.LoadStreamedTexture( ext_scene.textures[i].first );
}

void RenderApp::LoadPlaceholderTextures()
{
    auto& scene = m_renderer->GetScene();
    m_ph_normal_texture = scene.LoadStaticTexture( "resources/textures/default_deriv_normal.dds" );
    m_ph_specular_texture = scene.LoadStaticTexture( "resources/textures/default_spec.dds" );
    TextureID skybox_tex = scene.LoadStaticTexture( "D:/scenes/bistro/green_point_park_4k.DDS" );
    m_brdf_lut = scene.LoadStaticTexture( "D:/scenes/bistro/ibl/brdf_lut.DDS" );
    m_skybox_tf = scene.AddTransform();
    CubemapID skybox_cubemap = scene.AddCubemapFromTexture( skybox_tex );
    m_ph_skybox = scene.AddEnviromentMap( skybox_cubemap, m_skybox_tf );

    TextureID irradiance_map_tex = scene.LoadStaticTexture( "D:/scenes/bistro/ibl/irradiance.DDS" );
    m_renderer->SetIrradianceMap( scene.AddCubemapFromTexture( irradiance_map_tex ) );
    m_renderer->SetReflectionProbe( scene.LoadCubemap( "D:/scenes/bistro/ibl/reflection_probe_cm.dds" ) );
}

void RenderApp::BuildGeometry( ImportedScene& ext_scene )
{
    auto& scene = m_renderer->GetScene();

    for ( auto& mesh : ext_scene.meshes )
    {
        mesh.mesh_id = scene.LoadStaticMesh(
            mesh.name,
            mesh.vertices,
            mesh.indices );

        for ( auto& submesh : mesh.submeshes )
        {
            submesh.submesh_id = scene.AddSubmesh( mesh.mesh_id,
                                                   StaticSubmesh::Data{ uint32_t( submesh.nindices ),
                                                                        uint32_t( submesh.index_offset ),
                                                                        0 } );
        }
    }
}

void RenderApp::BuildMaterials( ImportedScene& ext_scene )
{
    auto& scene = m_renderer->GetScene();

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

        textures.preintegrated_brdf = m_brdf_lut;

		ext_material.material_id = scene.AddMaterial( textures, XMFLOAT3( 0.03f, 0.03f, 0.03f ), XMFLOAT3( 1, 1, 1 ) );
    }
}

void RenderApp::BuildRenderItems( const ImportedScene& ext_scene )
{
    auto& scene = m_renderer->GetScene();

    auto& world = scene.GetWorld();

    for ( const auto& ext_node : ext_scene.nodes )
    {
        DirectX::XMMATRIX tf = DirectX::XMLoadFloat4x4( &ext_node.transform );
        for ( const auto& ext_submesh : ext_scene.meshes[ext_node.mesh_idx].submeshes )
        {
            const int material_idx = ext_submesh.material_idx;
            if ( material_idx < 0 )
                throw SnowEngineException( "no material!" );

            MaterialID mat = ext_scene.materials[material_idx].second.material_id;
        
            auto entity = world.CreateEntity();
            world.AddComponent<Transform>( entity, Transform{ tf } );
            world.AddComponent<DrawableMesh>( entity, DrawableMesh{ ext_submesh.submesh_id, mat } );
        }
    }
}

void RenderApp::LoadingScreen::Init( SceneClientView& scene, TextureID normal_tex_id, TextureID specular_tex_id, EnvMapID skybox_id )
{
    Camera::Data camera_data;
    camera_data.type = Camera::Type::Perspective;
    camera_data.pos = DirectX::XMFLOAT3( 0, 1.7f, -3 );
    camera_data.dir = DirectX::XMFLOAT3( 0, -0.3f, 1 );
    XMFloat3Normalize( camera_data.dir );
    camera_data.up = DirectX::XMFLOAT3( 0, 1, 0 );
    camera_data.fov_y = XM_PIDIV4;
    camera_data.far_plane = 10000.0f;
    camera_data.near_plane = 0.1f;

	m_camera = scene.GetWorld().CreateEntity();
	Camera& cam_component = scene.GetWorld().AddComponent<Camera>( m_camera );
	cam_component.ModifyData() = camera_data;
    cam_component.SetSkybox() = skybox_id;

    Light::Data light_data;
    light_data.type = Light::LightType::Parallel;
    light_data.dir = DirectX::XMFLOAT3( 1, 1, -1 );
    XMFloat3Normalize( light_data.dir );
    light_data.strength = getLightIrradiance( 70.0e3f, DirectX::XMFLOAT3( 1, 1, 1 ), 2.2f );
    m_light = scene.AddLight( light_data );
    scene.ModifyLight( m_light )->ModifyShadow() = Light::Shadow{ 512, 3.0f, MAX_CASCADE_SIZE };

    LoadCube( scene, normal_tex_id, specular_tex_id );
}

void RenderApp::LoadingScreen::Enable( SceneClientView& scene, OldRenderer& renderer )
{
    renderer.SetFrustumCullCamera( World::Entity::nullid );
    renderer.SetMainCamera( m_camera );
    Light* light = scene.ModifyLight( m_light );
    if ( ! light )
        throw SnowEngineException( "light not found!" );
    light->IsEnabled() = true;

    DrawableMesh* cube_drawable = scene.GetWorld().GetComponent<DrawableMesh>( m_cube );
    if ( ! cube_drawable )
        throw SnowEngineException( "cube not found!" );

    renderer.SetSkybox(false);

    cube_drawable->show = true;
}

void RenderApp::LoadingScreen::Disable( SceneClientView& scene, OldRenderer& renderer )
{
    renderer.SetMainCamera( World::Entity::nullid );
    Light* light = scene.ModifyLight( m_light );
    if ( ! light )
        throw SnowEngineException( "light not found!" );
    light->IsEnabled() = false;

    DrawableMesh* cube_drawable = scene.GetWorld().GetComponent<DrawableMesh>( m_cube );
    if ( ! cube_drawable )
        throw SnowEngineException( "cube not found!" );

    renderer.SetSkybox(true);

    cube_drawable->show = false;
}

void RenderApp::LoadingScreen::Update( SceneClientView& scene, float screen_width, float screen_height, const GameTimer& gt )
{
    OPTICK_EVENT();
    auto& world = scene.GetWorld();

    Camera* cam = world.GetComponent<Camera>( m_camera );
    if ( ! cam )
        throw SnowEngineException( "camera not found!" );

    auto& cam_data = cam->ModifyData();
    cam_data.aspect_ratio = screen_width / screen_height;
    
    Transform* tf = world.GetComponent<Transform>( m_cube );
    if ( ! tf )
        throw SnowEngineException( "cube does not have a Transform!" );

    m_theta = gt.TotalTime();
    XMFLOAT3 cube_eye_dir = SphericalToCartesian( 1, XM_PIDIV2, m_theta );

    auto scale = XMMatrixScaling( 0.1f, 0.1f, 0.1f );
    auto rotation = XMMatrixLookToLH( XMVectorZero(),
                                      XMLoadFloat3( &cube_eye_dir ),
                                      XMVectorSet( 0, 1, 0, 0 ) );
    auto translation = XMMatrixTranslation( 1.0f * cam_data.aspect_ratio, -0.3f, 0 );

    tf->local2world = scale * rotation * translation;    
}

void RenderApp::LoadingScreen::LoadCube( SceneClientView& scene, TextureID normal_tex_id, TextureID specular_tex_id )
{
    TextureID loading_cube_texture = scene.LoadStaticTexture( "resources/textures/loading_box_base.dds" );
    MaterialID cube_material_id = scene.AddMaterial( MaterialPBR::TextureIds{ loading_cube_texture, normal_tex_id, specular_tex_id },
                                                     DirectX::XMFLOAT3( 0.3f, 0.3f, 0.3f ), DirectX::XMFLOAT3( 1, 1, 1 ), Identity4x4 );

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

    auto& world = scene.GetWorld();
    m_cube = world.CreateEntity();
    world.AddComponent<Transform>( m_cube, Transform{ DirectX::XMMatrixIdentity() } );
    world.AddComponent<DrawableMesh>( m_cube, DrawableMesh{ submesh_id, cube_material_id } );
}
