#pragma once

#include "RenderData.h"
#include "SceneImporter.h"
#include "FrameResource.h"

#include "StagingDescriptorHeap.h"

#include "ParallelSplitShadowMapping.h"

#include "GPUTaskQueue.h"

#include "Framegraph.h"
#include "ForwardPassNode.h"
#include "BlurSSAONode.h"
#include "DepthPrepassNode.h"
#include "ShadowPassNode.h"
#include "PSSMNode.h"
#include "HBAONode.h"
#include "ToneMapNode.h"
#include "UIPassNode.h"
#include "SkyboxNode.h"

#include "SceneManager.h"
#include "ForwardCBProvider.h"

// throws SnowEngineExceptions and DxExceptions for non-recoverable faults
class OldRenderer
{
public:
	
	OldRenderer( HWND main_hwnd, uint32_t screen_width, uint32_t screen_height );
	~OldRenderer();

	void Init( );
	
	struct Context
	{
		bool wireframe_mode;
		bool taa_enabled;
	};
	void Draw( const Context& ctx );
	void NewGUIFrame();
	void Resize( uint32_t new_width, uint32_t new_height );

	// getters/setters
	D3D12_VIEWPORT& ScreenViewport() { return m_screen_viewport; }
	const D3D12_VIEWPORT& ScreenViewport() const { return m_screen_viewport; }
	D3D12_RECT& ScissorRect() { return m_scissor_rect; }
	const D3D12_RECT& ScissorRect() const { return m_scissor_rect; }

	struct TonemapSettings
	{
		float max_luminance = 9000; // nits
		float min_luminance = 1.e-2f;
		bool blend_luminance = false;
	} m_tonemap_settings;

	HBAOPass::Settings m_hbao_settings;
	ParallelSplitShadowMapping& PSSM() noexcept { return m_pssm; }
	const ParallelSplitShadowMapping& PSSM() const noexcept { return m_pssm; }

	SceneClientView& GetScene() { return m_scene_manager->GetScene(); }

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

	// windows stuff
	HWND m_main_hwnd = nullptr;
	uint32_t m_client_width = 800;
	uint32_t m_client_height = 600;

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

	StaticMeshID m_geom_id = StaticMeshID::nullid;
	MaterialID m_placeholder_material = MaterialID::nullid;
	CameraID m_main_camera_id = CameraID::nullid;
	CameraID m_frustrum_cull_camera_id = CameraID::nullid;

	CubemapID m_irradiance_map = CubemapID::nullid;
	ComPtr<ID3D12Resource> m_reflection_probe_res;
	TextureID m_brdf_lut = TextureID::nullid;
	DescriptorTableID m_ibl_table = DescriptorTableID::nullid;

	std::unique_ptr<SceneManager> m_scene_manager;

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

	// framegraph
	ParallelSplitShadowMapping m_pssm;

	using FramegraphInstance =
		Framegraph
		<
			DepthPrepassNode,
			ShadowPassNode,
			PSSMNode,
			ForwardPassNode,
			SkyboxNode,
			HBAONode,
			BlurSSAONode,
			ToneMapNode,
			UIPassNode
		>;

	FramegraphInstance m_framegraph;

	// cmd lists
	ComPtr<ID3D12GraphicsCommandList> m_cmd_list = nullptr;

	// special cmd allocators
	ComPtr<ID3D12CommandAllocator> m_direct_cmd_allocator = nullptr;

	// postprocessing
	std::unique_ptr<DynamicTexture> m_fp_backbuffer;
	std::unique_ptr<DynamicTexture> m_ambient_lighting;
	std::unique_ptr<DynamicTexture> m_normals;
	std::unique_ptr<DynamicTexture> m_ssao;
	std::unique_ptr<DynamicTexture> m_ssao_blurred;
	std::unique_ptr<DynamicTexture> m_ssao_blurred_transposed;

	std::unique_ptr<ForwardCBProvider> m_forward_cb_provider;

	// methods
	void CreateDevice();
	void CreateBaseCommandObjects();
	void CreateSwapChain();
	void RecreateSwapChainAndDepthBuffers( uint32_t new_width, uint32_t new_height );
	void CreateTransientTextures();
	void ResizeTransientTextures();

	void BuildRtvAndDsvDescriptorHeaps();
	void BuildUIDescriptorHeap( );
	void InitImgui();

	void BuildFrameResources();
	void BuildPasses();
	
	void BindSkybox( EnvMapID skybox );

	void EndFrame(); // call at the end of the frame to wait for next available frame resource

	ID3D12Resource* CurrentBackBuffer();
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;
	const DescriptorTableBakery& DescriptorTables() const { return m_scene_manager->GetDescriptorTables(); }
	DescriptorTableBakery& DescriptorTables() { return m_scene_manager->GetDescriptorTables(); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle( DescriptorTableID id ) const { return DescriptorTables().GetTable( id )->gpu_handle; }
};
