#pragma once

#include <future>

#include <dxtk12/Keyboard.h>
#include <imgui/imgui.h>
#include "imgui_impl/imgui_impl_win32.h"
#include "imgui_impl/imgui_impl_dx12.h"
#include "Console.h"
#include <DirectXMath.h>

#include "D3DApp.h"
#include "DescriptorHeap.h"
#include "SceneImporter.h"

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
    void ReadKeyboardState( const GameTimer& gt );

    virtual void Draw( const GameTimer& gt ) override;

    virtual void OnMouseDown( WPARAM btnState, int x, int y ) override;
    virtual void OnMouseUp( WPARAM btnState, int x, int y ) override;
    virtual void OnMouseMove( WPARAM btnState, int x, int y ) override;

    virtual LRESULT MsgProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam ) override;

    // scene loading
    ImportedScene LoadScene( const std::string& filename );
    void InitScene( ImportedScene scene );

    void BuildGeometry( ImportedScene& ext_scene );
    void BuildMaterials( ImportedScene& ext_scene );
    void BuildRenderItems( const ImportedScene& ext_scene );
    void LoadAndBuildTextures( ImportedScene& ext_scene, bool flush_per_texture );
    void LoadPlaceholderTextures();

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

    CubemapID m_reflection_probe = CubemapID::nullid;
    CubemapID m_irradiance_map = CubemapID::nullid;
    TextureID m_brdf_lut = TextureID::nullid;

    CameraID m_camera = CameraID::nullid;
    CameraID m_dbg_frustum_camera = CameraID::nullid;
    bool m_dbg_use_separate_camera = false;
    LightID m_sun = LightID::nullid;

    class LoadingScreen
    {
    public:
        void Init( SceneClientView& scene, TextureID normal_tex_id, TextureID specular_tex_id, EnvMapID skybox_id );
        void Enable( SceneClientView& scene, OldRenderer& renderer );
        void Disable( SceneClientView& scene, OldRenderer& renderer );
        void Update( SceneClientView& scene, float screen_width, float screen_height, const GameTimer& gt );

    private:
        void LoadCube( SceneClientView& scene, TextureID normal_tex_id, TextureID specular_tex_id );

        World::Entity m_cube = World::Entity::nullid;
        CameraID m_camera = CameraID::nullid;
        LightID m_light = LightID::nullid;
        float m_theta = 0;
    } m_loading_screen;

    std::future<ImportedScene> m_is_scene_loaded;

    std::unique_ptr<OldRenderer> m_renderer = nullptr;
    
    float m_theta = -0.1f * DirectX::XM_PI;
    float m_phi = 0.8f * DirectX::XM_PIDIV2;
    DirectX::XMFLOAT3 m_camera_pos = DirectX::XMFLOAT3( 13.5f, 4.13f, -2.73f );
    float m_camera_speed = 5.0f;

    float m_sun_theta = -1.852f;
    float m_sun_phi = 2.496f;
    float m_sun_illuminance = 90.0e3f; // lux
    float m_sky_phi = DirectX::XM_PI;
    float m_sky_radiance_factor = 18.0f;
    DirectX::XMFLOAT3 m_sun_color_corrected = DirectX::XMFLOAT3( 1.0f, 232.0f /255.0f, 213.0f /255.0f ); // gamma == 2.2

    bool m_is_console_opened = true;
    bool m_tilda_was_pressed_last_frame = false;
    ExampleAppConsole m_console;

    // inputs
    std::unique_ptr<DirectX::Keyboard> m_keyboard;
    POINT m_last_mouse_pos;
    LPSTR m_cmd_line;
};
