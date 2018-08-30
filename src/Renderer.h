#pragma once

#include "RenderData.h"
#include "SceneImporter.h"
#include "FrameResource.h"

#include "TemporalAA.h"
#include "ToneMappingPass.h"

#include "Pipeline.h"
#include "PipelineNodes.h"

class ForwardLightingPass;
class DepthOnlyPass;

// throws SnowEngineExceptions and DxExceptions for non-recoverable faults
class Renderer
{
public:
	
	Renderer( HWND main_hwnd, size_t screen_width, size_t screen_height );

	void InitD3D( );
	void Init( const ImportedScene& ext_scene );
	
	// returns draw_gui retval
	struct Context
	{
		bool wireframe_mode;
		bool taa_enabled;
	};
	void Draw( const Context& ctx );
	void NewGUIFrame();
	void Resize( size_t new_width, size_t new_height );

	// getters/setters
	D3D12_VIEWPORT& ScreenViewport() { return m_screen_viewport; }
	const D3D12_VIEWPORT& ScreenViewport() const { return m_screen_viewport; }
	D3D12_RECT& ScissorRect() { return m_scissor_rect; }
	const D3D12_RECT& ScissorRect() const { return m_scissor_rect; }
	TemporalAA& TemporalAntiAliasing() { return m_taa; }

	struct TonemapSettings
	{
		float max_luminance = 20000;
		float min_luminance = 1.e-2f;
		bool blend_luminance = false;
	} m_tonemap_settings;

	// for update. Todo: something smarter
	FrameResource& GetCurFrameResources() { return *m_cur_frame_resource; }
	RenderSceneContext& GetScene() { return m_scene; }

private:
	// data

	// windows stuff
	HWND m_main_hwnd = nullptr;
	size_t m_client_width = 800;
	size_t m_client_height = 600;

	// d3d stuff
	ComPtr<IDXGIFactory4> m_dxgi_factory = nullptr;
	ComPtr<ID3D12Device> m_d3d_device = nullptr;
	ComPtr<ID3D12Fence> m_fence = nullptr;
	UINT64 m_current_fence = 0;

	ComPtr<IDXGISwapChain> m_swap_chain = nullptr;
	static constexpr int SwapChainBufferCount = 2;
	int m_curr_back_buff = 0;
	ComPtr<ID3D12Resource> m_swap_chain_buffer[SwapChainBufferCount];
	std::optional<Descriptor> m_back_buffer_rtv[SwapChainBufferCount];
	ComPtr<ID3D12Resource> m_depth_stencil_buffer;
	std::optional<Descriptor> m_back_buffer_dsv;

	DXGI_FORMAT m_back_buffer_format = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_depth_stencil_format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	ComPtr<ID3D12CommandQueue> m_cmd_queue = nullptr;
	D3D12_VIEWPORT m_screen_viewport;
	D3D12_RECT m_scissor_rect;

	RenderSceneContext m_scene;

	// descriptor heaps
	std::unique_ptr<DescriptorHeap> m_srv_heap = nullptr;
	std::unique_ptr<DescriptorHeap> m_dsv_heap = nullptr;
	std::unique_ptr<DescriptorHeap> m_rtv_heap = nullptr;

	size_t m_cbv_srv_uav_size = 0;
	size_t m_dsv_size = 0;
	size_t m_rtv_size = 0;

	// ui font descriptor
	std::unique_ptr<Descriptor> m_ui_font_desc = nullptr;

	// resources
	static constexpr int FrameResourceCount = 3;
	std::vector<FrameResource> m_frame_resources;
	FrameResource* m_cur_frame_resource = nullptr;
	int m_cur_fr_idx = 0;
	static constexpr int PassCount = 2;

	// pipeline
	std::unique_ptr<ForwardLightingPass> m_forward_pass = nullptr;
	std::unique_ptr<DepthOnlyPass> m_depth_pass = nullptr;
	std::unique_ptr<TemporalBlendPass> m_txaa_pass = nullptr;
	std::unique_ptr<ToneMappingPass> m_tonemap_pass = nullptr;

	using PipelineInstance = Pipeline<ShadowPassNode, ForwardPassNode, ToneMapPassNode, UIPassNode>;
	PipelineInstance m_pipeline;

	// cmd lists
	ComPtr<ID3D12GraphicsCommandList> m_cmd_list = nullptr;
	ComPtr<ID3D12GraphicsCommandList> m_sm_cmd_lst = nullptr;

	// special cmd allocators
	ComPtr<ID3D12CommandAllocator> m_direct_cmd_allocator = nullptr;

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
	DynamicTexture m_fp_backbuffer;
	ComPtr<ID3D12RootSignature> m_tonemap_root_signature = nullptr;
	ComPtr<ID3D12PipelineState> m_tonemap_pso = nullptr;

	TemporalAA m_taa;

	// methods
	void CreateBaseCommandObjects();
	void CreateSwapChain();

	void BuildRtvAndDsvDescriptorHeaps();
	void BuildSrvDescriptorHeap( const ImportedScene& ext_scene );
	void RecreatePrevFrameTexture();
	void InitGUI();
	void LoadAndBuildTextures( const ImportedScene& ext_scene, bool flush_per_texture );

	void BuildGeometry( const ImportedScene& ext_scene );
	void BuildMaterials( const ImportedScene& ext_scene );
	void BuildRenderItems( const ImportedScene& ext_scene );
	void BuildFrameResources( );
	void BuildLights();
	void BuildConstantBuffers();
	void BuildPasses();

	void LoadStaticDDSTexture( const wchar_t* filename, const std::string& name );
	void CreateShadowMap( Texture& texture );
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> BuildStaticSamplers() const;

	template <DXGI_FORMAT index_format, class VCRange, class ICRange>
	std::unordered_map<std::string, SubmeshGeometry>& LoadStaticGeometry( std::string name, const VCRange& vertices, const ICRange& indices, ID3D12GraphicsCommandList* cmd_list );

	void DisposeUploaders();
	void DisposeCPUGeom();
	
	void EndFrame(); // call at the end of the frame to wait for next available frame resource
	void FlushCommandQueue();

	ID3D12Resource* CurrentBackBuffer();
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;
};