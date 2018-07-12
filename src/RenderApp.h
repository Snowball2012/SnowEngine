#pragma once

#include <Luna/MathHelper.h>
#include <Luna/UploadBuffer.h>

#include <dxtk12/Keyboard.h>
#include <imgui/imgui.h>
#include "imgui_impl/imgui_impl_win32.h"
#include "imgui_impl/imgui_impl_dx12.h"

#include "D3DApp.h"
#include "FrameResource.h"
#include "DescriptorHeap.h"
#include "SceneImporter.h"

#include "TemporalAA.h"

#include "Renderer.h"

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
	
	void UpdatePassConstants( const GameTimer& gt, Utils::UploadBuffer<PassConstants>& pass_cb );
	void UpdateLights( PassConstants& pc );
	void UpdateRenderItem( RenderItem& renderitem, Utils::UploadBuffer<ObjectConstants>& obj_cb );

	// input
	void ReadEventKeys();
	void ReadKeyboardState( const GameTimer& gt );

	virtual void Draw( const GameTimer& gt ) override;

	virtual void OnMouseDown( WPARAM btnState, int x, int y ) override;
	virtual void OnMouseUp( WPARAM btnState, int x, int y ) override;
	virtual void OnMouseMove( WPARAM btnState, int x, int y ) override;

	virtual LRESULT MsgProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam ) override;

	// scene loading
	void LoadModel( const std::string& filename );

	std::unique_ptr<Renderer> m_renderer = nullptr;

	// external geometry
	ImportedScene m_imported_scene;
	
	float m_theta = -0.1f * DirectX::XM_PI;
	float m_phi = 0.8f * DirectX::XM_PIDIV2;
	DirectX::XMFLOAT3 m_camera_pos = DirectX::XMFLOAT3( 13.5f, 4.13f, -2.73f );
	float m_camera_speed = 5.0f;

	float m_sun_theta = 0.75f * DirectX::XM_PI;
	float m_sun_phi = 1.6f * DirectX::XM_PIDIV2;

	bool m_wireframe_mode = false;

	// temporal AA
	bool m_taa_enabled = true;
	uint64_t m_cur_frame_idx = 0;

	// inputs
	std::unique_ptr<DirectX::Keyboard> m_keyboard;
	POINT m_last_mouse_pos;
	LPSTR m_cmd_line;
};
