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

class ForwardLightingPass;
class DepthOnlyPass;
class TemporalBlendPass;

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
	
	void UpdateAndWaitForFrameResource();
	// isolated from FrameResources logic
	void UpdatePassConstants( const GameTimer& gt, Utils::UploadBuffer<PassConstants>& pass_cb );
	void UpdateLights( PassConstants& pc );
	void UpdateRenderItem( RenderItem& renderitem, Utils::UploadBuffer<ObjectConstants>& obj_cb );
	void UpdateDynamicGeometry( Utils::UploadBuffer<Vertex>& dynamic_vb );

	// input
	void ReadEventKeys();
	void ReadKeyboardState( const GameTimer& gt );

	virtual void Draw( const GameTimer& gt ) override;

	void WaitForFence( UINT64 fence_val );

	virtual void OnMouseDown( WPARAM btnState, int x, int y ) override;
	virtual void OnMouseUp( WPARAM btnState, int x, int y ) override;
	virtual void OnMouseMove( WPARAM btnState, int x, int y ) override;

	virtual LRESULT MsgProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam ) override;

	// scene loading
	void LoadModel( const std::string& filename );

	// build functions
	void BuildPasses();

	void BuildDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildGeometry();
	void LoadAndBuildTextures();
	void LoadStaticDDSTexture( const wchar_t* filename, const std::string& name );
	void BuildMaterials();
	void BuildLights();
	void BuildRenderItems();
	void BuildFrameResources();
	void CreateShadowMap( Texture& texture );

	void RecreatePrevFrameTexture();

	void DisposeUploaders();
	void DisposeCPUGeom();

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> BuildStaticSamplers() const;

	// external geometry
	ImportedScene m_imported_scene;

	RenderSceneContext m_scene;

	// geometry
	template <DXGI_FORMAT index_format, class VCRange, class ICRange>
	std::unordered_map<std::string, SubmeshGeometry>& LoadStaticGeometry( std::string name, const VCRange& vertices, const ICRange& indices, ID3D12GraphicsCommandList* cmd_list );

	template <DXGI_FORMAT index_format, class ICRange>
	void LoadDynamicGeometryIndices( const ICRange& indices, ID3D12GraphicsCommandList* cmd_list );
	MeshGeometry m_dynamic_geometry;

	// descriptor heaps
	std::unique_ptr<DescriptorHeap> m_srv_heap = nullptr;
	std::unique_ptr<DescriptorHeap> m_dsv_heap = nullptr;

	// ui font descriptor
	std::unique_ptr<Descriptor> m_ui_font_desc = nullptr;

	// resources
	static constexpr int num_frame_resources = 3;
	std::vector<FrameResource> m_frame_resources;
	FrameResource* m_cur_frame_resource = nullptr;
	int m_cur_fr_idx = 0;
	static constexpr int num_passes = 2;

	// pipeline
	std::unique_ptr<ForwardLightingPass> m_forward_pass;
	std::unique_ptr<DepthOnlyPass> m_depth_pass;
	std::unique_ptr<TemporalBlendPass> m_txaa_pass;

	// cmd lists
	ComPtr<ID3D12GraphicsCommandList> m_sm_cmd_lst;

	// forward
	ComPtr<ID3D12RootSignature> m_forward_root_signature = nullptr;
	ComPtr<ID3D12PipelineState> m_forward_pso_main = nullptr;
	ComPtr<ID3D12PipelineState> m_forward_pso_wireframe = nullptr;

	// depth only
	ComPtr<ID3D12RootSignature> m_do_root_signature = nullptr;
	ComPtr<ID3D12PipelineState> m_do_pso = nullptr;

	// postprocessing
	ComPtr<ID3D12RootSignature> m_txaa_root_signature = nullptr;
	ComPtr<ID3D12PipelineState> m_txaa_pso = nullptr;
	Texture m_prev_frame_texture;
	Texture m_jittered_frame_texture;
	
	float m_theta = -0.1f * DirectX::XM_PI;
	float m_phi = 0.8f * DirectX::XM_PIDIV2;
	DirectX::XMFLOAT3 m_camera_pos = DirectX::XMFLOAT3( 13.5f, 4.13f, -2.73f );
	float m_camera_speed = 5.0f;

	float m_sun_theta = 0.75f * DirectX::XM_PI;
	float m_sun_phi = 1.6f * DirectX::XM_PIDIV2;

	bool m_wireframe_mode = false;

	// temporal AA
	bool m_enable_txaa = true;
	bool m_jitter_proj = true;
	float m_jitter_val = 1.0f;
	bool m_blend_prev_frame = true;
	float m_blend_fraction = 0.9f;
	float m_last_jitter[2] = { 0 };
	constexpr static float m_halton_23[8][2] = 
	{
		{ 1.f/2.f, 1.f /3.f },
		{ 1.f /4.f, 2.f /3.f },
		{ 3.f /4.f, 1.f /9.f },
		{ 1.f /8.f, 4.f /9.f },
		{ 5.f /8.f, 7.f /9.f },
		{ 3.f /8.f, 2.f /9.f },
		{ 7.f /8.f, 5.f /9.f },
		{ 1.f /16.f, 8.f /9.f }
	};
	uint64_t m_cur_frame_idx = 0;

	// inputs
	std::unique_ptr<DirectX::Keyboard> m_keyboard;
	POINT m_last_mouse_pos;
	LPSTR m_cmd_line;
};
