#pragma once

#include "RenderData.h"
#include "SceneImporter.h"
#include "FrameResource.h"

class ForwardLightingPass;
class DepthOnlyPass;
class TemporalBlendPass;

// throws SnowEngineExceptions for non-recoverable faults
class Renderer
{
public:
	struct Context
	{
		ComPtr<ID3D12GraphicsCommandList> direct_cmd_list;
		ComPtr<ID3D12CommandAllocator> direct_cmd_alloc;
		ComPtr<ID3D12CommandQueue> graphics_cmd_queue;
		DXGI_FORMAT back_buffer_format;
		size_t cbv_srv_uav_size;
		size_t dsv_size;
		HWND main_hwnd;
	};
	Renderer( ComPtr<ID3D12Device> device, const Context& ctx )
		: m_d3d_device( std::move( device ) )
	{}

	void Init( const ImportedScene& ext_scene );

	using fnDrawGUI = std::function<bool( void )>;
	void Draw( const fnDrawGUI& draw_gui );

private:
	// data

	ComPtr<ID3D12Device> m_d3d_device = nullptr;
	HWND m_main_hwnd = nullptr;
	DXGI_FORMAT m_back_buffer_format = DXGI_FORMAT_UNKNOWN;
	ComPtr<ID3D12CommandQueue> m_cmd_queue = nullptr;

	RenderSceneContext m_scene;

	// descriptor heaps
	std::unique_ptr<DescriptorHeap> m_srv_heap = nullptr;
	std::unique_ptr<DescriptorHeap> m_dsv_heap = nullptr;

	size_t m_cbv_srv_uav_size = 0;
	size_t m_dsv_size = 0;

	// ui font descriptor
	std::unique_ptr<Descriptor> m_ui_font_desc = nullptr;

	// resources
	static constexpr int num_frame_resources = 3;
	std::vector<FrameResource> m_frame_resources;
	FrameResource* m_cur_frame_resource = nullptr;
	int m_cur_fr_idx = 0;
	static constexpr int num_passes = 2;

	// pipeline
	std::unique_ptr<ForwardLightingPass> m_forward_pass = nullptr;
	std::unique_ptr<DepthOnlyPass> m_depth_pass = nullptr;
	std::unique_ptr<TemporalBlendPass> m_txaa_pass = nullptr;

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

	// methods
	void BuildDescriptorHeaps( const ImportedScene& ext_scene );
	void RecreatePrevFrameTexture();
	void InitGUI();
	void LoadAndBuildTextures( const ImportedScene& ext_scene, bool flush_per_texture );
	void LoadStaticDDSTexture( const wchar_t* filename, const std::string& name );

	void FlushCommandQueue();

	ID3D12Resource* CurrentBackBuffer();
};