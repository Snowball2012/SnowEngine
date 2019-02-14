#pragma once

#include <future>

#include <Luna/MathHelper.h>
#include <Luna/UploadBuffer.h>

#include <dxtk12/Keyboard.h>
#include <imgui/imgui.h>
#include "imgui_impl/imgui_impl_win32.h"
#include "imgui_impl/imgui_impl_dx12.h"
#include <DirectXMath.h>

#include "D3DApp.h"
#include "FrameResource.h"
#include "DescriptorHeap.h"
#include "SceneImporter.h"

#include "TemporalAA.h"

#include "OldRenderer.h"

class RenderApp: public D3DApp
{
public:
	RenderApp( HINSTANCE hinstance, LPSTR cmd_line );
	RenderApp( const RenderApp& rhs ) = delete;
	RenderApp& operator=( const RenderApp& rhs ) = delete;
	~RenderApp();

	// Inherited via D3DApp
	virtual bool Initialize() override;

private:
	// D3DApp
	virtual void OnResize() override;

	virtual void Update( const GameTimer& gt ) override;

	void UpdateGUI();
	void UpdateCamera();
	void UpdateLights();

	// input
	void ReadEventKeys();
	void ReadKeyboardState( const GameTimer& gt );

	virtual void Draw( const GameTimer& gt ) override;

	virtual void OnMouseDown( WPARAM btnState, int x, int y ) override;
	virtual void OnMouseUp( WPARAM btnState, int x, int y ) override;
	virtual void OnMouseMove( WPARAM btnState, int x, int y ) override;

	virtual LRESULT MsgProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam ) override;

	// scene loading
	void LoadScene( const std::string& filename );
	void InitScene( );

	void BuildGeometry( ImportedScene& ext_scene );
	void BuildMaterials( ImportedScene& ext_scene );
	void BuildRenderItems( const ImportedScene& ext_scene );
	void LoadAndBuildTextures( ImportedScene& ext_scene, bool flush_per_texture );
	void LoadPlaceholderTextures();
	void ReleaseIntermediateSceneMemory();

	enum class State
	{
		Loading,
		Main
	};

	State m_cur_state = State::Loading;

	// Placeholders and loading state scene objects
	TextureID m_ph_normal_texture = TextureID::nullid;
	TextureID m_ph_specular_texture = TextureID::nullid;
	EnvMapID m_ph_skybox = EnvMapID::nullid;
	TransformID m_skybox_tf = TransformID::nullid;

	class LoadingScreen
	{
	public:
		void Init( SceneClientView& scene, TextureID normal_tex_id, TextureID specular_tex_id, EnvMapID skybox_id );
		void Enable( SceneClientView& scene, OldRenderer& renderer );
		void Disable( SceneClientView& scene, OldRenderer& renderer );
		void Update( SceneClientView& scene, float screen_width, float screen_height, const GameTimer& gt );

	private:
		void LoadCube( SceneClientView& scene, TextureID normal_tex_id, TextureID specular_tex_id );

		MeshInstanceID m_cube = MeshInstanceID::nullid;
		CameraID m_camera = CameraID::nullid;
		LightID m_light = LightID::nullid;
		float m_theta = 0;
	} m_loading_screen;

	std::future<void> m_is_scene_loaded;

	std::unique_ptr<OldRenderer> m_renderer = nullptr;
	CameraID m_camera = CameraID::nullid;
	CameraID m_dbg_frustrum_camera = CameraID::nullid;
	bool m_dbg_use_separate_camera = false;
	LightID m_sun = LightID::nullid;


	CubemapID m_reflection_probe = CubemapID::nullid;
	CubemapID m_irradiance_map = CubemapID::nullid;
	TextureID m_brdf_lut = TextureID::nullid;

	// external geometry
	ImportedScene m_imported_scene;
	
	float m_theta = -0.1f * DirectX::XM_PI;
	float m_phi = 0.8f * DirectX::XM_PIDIV2;
	DirectX::XMFLOAT3 m_camera_pos = DirectX::XMFLOAT3( 13.5f, 4.13f, -2.73f );
	float m_camera_speed = 5.0f;

	float m_sun_theta = -1.852f;
	float m_sun_phi = 2.496f;
	float m_sun_illuminance = 90.0e3f; // lux
	float m_sky_phi = DirectX::XM_PI;
	float m_sky_radiance_factor = 10.0f;
	DirectX::XMFLOAT3 m_sun_color_corrected = DirectX::XMFLOAT3( 1.0f, 232.0f /255.0f, 213.0f /255.0f ); // gamma == 2.2

	bool m_wireframe_mode = false;

	// temporal AA
	bool m_taa_enabled = false;

	// inputs
	std::unique_ptr<DirectX::Keyboard> m_keyboard;
	POINT m_last_mouse_pos;
	LPSTR m_cmd_line;
};
