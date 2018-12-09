#pragma once

#include "RenderData.h"
#include "SceneImporter.h"
#include "FrameResource.h"

#include "StagingDescriptorHeap.h"

#include "TemporalAA.h"
#include "ToneMappingPass.h"
#include "HBAOPass.h"

#include "GPUTaskQueue.h"

#include "Pipeline.h"
#include "PipelineNodes.h"

#include "SceneManager.h"
#include "ForwardCBProvider.h"

class ForwardLightingPass;
class DepthOnlyPass;

// throws SnowEngineExceptions and DxExceptions for non-recoverable faults
class Renderer
{
public:
	
	Renderer( HWND main_hwnd, size_t screen_width, size_t screen_height );
	~Renderer();

	void InitD3D( );
	
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

	struct TonemapSettings
	{
		float max_luminance = 20000;
		float min_luminance = 1.e-2f;
		bool blend_luminance = false;
	} m_tonemap_settings;

	HBAOPass::Settings m_hbao_settings;

	// for update. Todo: something smarter
	FrameResource& GetCurFrameResources() { return *m_cur_frame_resource; }
	RenderSceneContext& GetScene() { return m_scene; }

	SceneClientView& GetSceneView() { return m_scene_manager->GetScene(); }

	// returns false if camera doesn't exist
	bool SetMainCamera( CameraID id );
	bool SetFrustrumCullCamera( CameraID id ); // Renderer will use main camera if this one is not specified, mainly for debug purposes

	struct PerformanceStats
	{
		TextureStreamer::Stats tex_streamer;
	};
	
	PerformanceStats GetPerformanceStats() const noexcept;

private:
	using DescriptorTableID = DescriptorTableBakery::TableID;

	// data

	// windows stuff
	HWND m_main_hwnd = nullptr;
	size_t m_client_width = 800;
	size_t m_client_height = 600;

	// d3d stuff
	ComPtr<IDXGIFactory4> m_dxgi_factory = nullptr;
	ComPtr<ID3D12Device> m_d3d_device = nullptr;

	std::unique_ptr<GPUTaskQueue> m_graphics_queue = nullptr;
	std::unique_ptr<GPUTaskQueue> m_copy_queue = nullptr;
	std::unique_ptr<GPUTaskQueue> m_compute_queue = nullptr;

	ComPtr<IDXGISwapChain> m_swap_chain = nullptr;
	static constexpr int SwapChainBufferCount = 2;
	int m_curr_back_buff = 0;
	ComPtr<ID3D12Resource> m_swap_chain_buffer[SwapChainBufferCount] = { nullptr };
	std::optional<Descriptor> m_back_buffer_rtv[SwapChainBufferCount] = { std::nullopt };
	ComPtr<ID3D12Resource> m_depth_stencil_buffer;
	std::optional<Descriptor> m_back_buffer_dsv;
	DescriptorTableID m_depth_buffer_srv = DescriptorTableID::nullid;

	DXGI_FORMAT m_back_buffer_format = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_depth_stencil_format = DXGI_FORMAT_D32_FLOAT;
	D3D12_VIEWPORT m_screen_viewport;
	D3D12_RECT m_scissor_rect;

	RenderSceneContext m_scene;
	StaticMeshID m_geom_id = StaticMeshID::nullid;
	MaterialID m_placeholder_material = MaterialID::nullid;
	CameraID m_main_camera_id = CameraID::nullid;
	CameraID m_frustrum_cull_camera_id = CameraID::nullid;

	std::unique_ptr<SceneManager> m_scene_manager;
	std::unique_ptr<ForwardCBProvider> m_forward_cb_provider;

	// descriptor heaps
	std::unique_ptr<DescriptorHeap> m_srv_ui_heap = nullptr;
	std::unique_ptr<StagingDescriptorHeap> m_dsv_heap = nullptr;
	std::unique_ptr<StagingDescriptorHeap> m_rtv_heap = nullptr;

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
	std::unique_ptr<DepthOnlyPass> m_shadow_pass = nullptr;
	std::unique_ptr<DepthOnlyPass> m_depth_prepass = nullptr;
	std::unique_ptr<ToneMappingPass> m_tonemap_pass = nullptr;
	std::unique_ptr<HBAOPass> m_hbao_pass = nullptr;
	std::unique_ptr<DepthAwareBlurPass> m_blur_pass = nullptr;

	using PipelineInstance =
		Pipeline
		<
			DepthPrepassNode,
			ShadowPassNode,
			ForwardPassNode,
			HBAOGeneratorNode,
			BlurSSAONode,
			ToneMapPassNode,
			UIPassNode
		>;

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
	ComPtr<ID3D12PipelineState> m_z_prepass_pso = nullptr;

	// postprocessing
	DynamicTexture m_fp_backbuffer;
	DynamicTexture m_ambient_lighting;
	DynamicTexture m_normals;
	DynamicTexture m_ssao;
	DynamicTexture m_ssao_blurred;

	ComPtr<ID3D12RootSignature> m_tonemap_root_signature = nullptr;
	ComPtr<ID3D12PipelineState> m_tonemap_pso = nullptr;
	ComPtr<ID3D12RootSignature> m_hbao_root_signature = nullptr;
	ComPtr<ID3D12PipelineState> m_hbao_pso = nullptr;
	ComPtr<ID3D12RootSignature> m_blur_root_signature = nullptr;
	ComPtr<ID3D12PipelineState> m_blur_pso = nullptr;

	// methods
	void CreateDevice();
	void CreateBaseCommandObjects();
	void CreateSwapChain();
	void RecreateSwapChainAndDepthBuffers( size_t new_width, size_t new_height );
	void RecreatePrevFrameTexture( bool create_tables );

	void BuildRtvAndDsvDescriptorHeaps();
	void BuildUIDescriptorHeap( );
	void InitImgui();

	void BuildFrameResources();
	void BuildPasses();
	
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> BuildStaticSamplers() const;
	
	void EndFrame(); // call at the end of the frame to wait for next available frame resource

	ID3D12Resource* CurrentBackBuffer();
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;
	const DescriptorTableBakery& DescriptorTables() const { return m_scene_manager->GetDescriptorTables(); }
	DescriptorTableBakery& DescriptorTables() { return m_scene_manager->GetDescriptorTables(); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle( DescriptorTableID id ) const { return DescriptorTables().GetTable( id )->gpu_handle; }
};
