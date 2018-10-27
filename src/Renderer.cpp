#include "stdafx.h"

#include "Renderer.h"

#include <imgui/imgui.h>
#include "imgui_impl/imgui_impl_win32.h"
#include "imgui_impl/imgui_impl_dx12.h"

#include "GeomGeneration.h"

#include "ForwardLightingPass.h"
#include "DepthOnlyPass.h"
#include "TemporalBlendPass.h"
#include "ToneMappingPass.h"

#include <dxtk12/DDSTextureLoader.h>
#include <dxtk12/DirectXHelpers.h>

#include <d3dcompiler.h>
#include <d3d12sdklayers.h>

#include <future>

using namespace DirectX;

Renderer::Renderer( HWND main_hwnd, size_t screen_width, size_t screen_height )
	: m_main_hwnd( main_hwnd ), m_client_width( screen_width ), m_client_height( screen_height )
{}

Renderer::~Renderer()
{
	for ( auto& desc : m_back_buffer_rtv )
		desc.reset();
	m_back_buffer_dsv.reset();
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	m_ui_font_desc.reset();
	m_fp_backbuffer.rtv.reset();
	m_ambient_lighting.rtv.reset();
	m_normals.rtv.reset();
	m_ssao.rtv.reset();

	for ( auto& shadow_map : m_scene.shadow_maps )
		shadow_map.second.dsv.reset();
}

void Renderer::InitD3D()
{
	CreateDevice();

	m_graphics_queue = std::make_unique<GPUTaskQueue>( *m_d3d_device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT );
	m_copy_queue = std::make_unique<GPUTaskQueue>( *m_d3d_device.Get(), D3D12_COMMAND_LIST_TYPE_COPY );
	m_compute_queue = std::make_unique<GPUTaskQueue>( *m_d3d_device.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE );

	m_rtv_size = m_d3d_device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
	m_dsv_size = m_d3d_device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_DSV );
	m_cbv_srv_uav_size = m_d3d_device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

	CreateBaseCommandObjects();
	CreateSwapChain();

	InitImgui();

	m_scene_manager = std::make_unique<SceneManager>( m_d3d_device, FrameResourceCount, m_copy_queue.get() );

	BuildRtvAndDsvDescriptorHeaps();
	RecreateSwapChainAndDepthBuffers( m_client_width, m_client_height );
	RecreatePrevFrameTexture( true );
}

void Renderer::Init( const ImportedScene& ext_scene )
{
	ThrowIfFailed( m_cmd_list->Reset( m_direct_cmd_allocator.Get(), nullptr ) );

	RecreatePrevFrameTexture( false );

	LoadAndBuildTextures( ext_scene, true );
	BuildGeometry( ext_scene );

	BuildMaterials( ext_scene );
	BuildRenderItems( ext_scene );
	BuildFrameResources( );
	BuildLights();

	BuildConstantBuffers();
	BuildPasses();

	ThrowIfFailed( m_cmd_list->Close() );
	ID3D12CommandList* cmd_lists[] = { m_cmd_list.Get() };
	m_graphics_queue->GetCmdQueue()->ExecuteCommandLists( _countof( cmd_lists ), cmd_lists );
	m_graphics_queue->Flush();

	m_scene_manager->UpdatePipelineBindings();
	m_scene_manager->FlushAllOperations();
	m_scene_manager->UpdatePipelineBindings();
	m_scene_manager->FlushAllOperations();
}

void Renderer::Draw( const Context& ctx )
{
	if ( m_pipeline.IsRebuildNeeded() )
		m_pipeline.RebuildPipeline();

	m_scene_manager->UpdatePipelineBindings();

	for ( auto& renderitem : m_scene.renderitems )
	{
		const StaticMesh& geom = GetSceneView().GetROScene().AllStaticMeshes()[renderitem.geom];
		assert( geom.IsLoaded() );
		renderitem.ibv = geom.IndexBufferView();
		renderitem.vbv = geom.VertexBufferView();
		renderitem.tf_addr = GetSceneView().GetROScene().AllTransforms()[renderitem.tf_id].GPUView();
		const MaterialPBR& material = GetSceneView().GetROScene().AllMaterials()[renderitem.material];
		renderitem.mat_table = material.DescriptorTable();
		renderitem.mat_cb = material.GPUConstantBuffer();
		const StaticSubmesh& submesh = GetSceneView().GetROScene().AllStaticSubmeshes()[renderitem.submesh];
		renderitem.index_count = submesh.DrawArgs().idx_cnt;
		renderitem.index_offset = submesh.DrawArgs().start_index_loc;
		renderitem.vertex_offset = submesh.DrawArgs().base_vertex_loc;
	}

	for ( auto& shadow_map : m_scene.shadow_maps )
		shadow_map.second.frame_srv = GetGPUHandle( shadow_map.second.srv );

	// Reuse memory associated with command recording
	// We can only reset when the associated command lists have finished execution on GPU
	for ( auto& allocator : m_cur_frame_resource->cmd_list_allocs )
		ThrowIfFailed( allocator->Reset() );

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList. Reusing the command list reuses memory
	ThrowIfFailed( m_cmd_list->Reset( m_cur_frame_resource->cmd_list_allocs[0].Get(), m_forward_pso_main.Get() ) );

	CD3DX12_RESOURCE_BARRIER rtv_barriers[7];
	rtv_barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition( CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET );
	rtv_barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition( m_fp_backbuffer.texture_gpu.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET );
	rtv_barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition( m_ambient_lighting.texture_gpu.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET );
	rtv_barriers[3] = CD3DX12_RESOURCE_BARRIER::Transition( m_normals.texture_gpu.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET );
	rtv_barriers[4] = CD3DX12_RESOURCE_BARRIER::Transition( m_ssao.texture_gpu.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET );
	rtv_barriers[5] = CD3DX12_RESOURCE_BARRIER::Transition( m_depth_stencil_buffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	rtv_barriers[6] = CD3DX12_RESOURCE_BARRIER::Transition( m_scene.lights["sun"].shadow_map->texture_gpu.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	
	m_cmd_list->ResourceBarrier( 7, rtv_barriers );
	ID3D12DescriptorHeap* heaps[] = { m_scene_manager->GetDescriptorTables().CurrentGPUHeap().Get() };
	m_cmd_list->SetDescriptorHeaps( 1, heaps );

	std::vector<GPULight*> lights_with_shadow;
	std::vector<D3D12_GPU_VIRTUAL_ADDRESS> sm_pass_cbs;

	{
		ShadowCasters casters;
		casters.casters = &m_scene.renderitems;
		m_pipeline.SetRes( casters );

		ShadowProducers producers;
		lights_with_shadow.push_back( &m_scene.lights["sun"] );
		producers.lights = &lights_with_shadow;
		sm_pass_cbs.push_back( m_cur_frame_resource->pass_cb->Resource()->GetGPUVirtualAddress() + Utils::CalcConstantBufferByteSize( sizeof( PassConstants ) ) );
		producers.pass_cbs = &sm_pass_cbs;
		m_pipeline.SetRes( producers );

		ShadowMapStorage smstorage;
		smstorage.sm_storage = &lights_with_shadow;
		m_pipeline.SetRes( smstorage );

		ForwardPassCB pass_cb{ m_cur_frame_resource->pass_cb->Resource()->GetGPUVirtualAddress() };
		m_pipeline.SetRes( pass_cb );

		HDRColorStorage hdr_buffer;
		hdr_buffer.resource = m_fp_backbuffer.texture_gpu.Get();
		hdr_buffer.rtv = m_fp_backbuffer.rtv->HandleCPU();
		hdr_buffer.srv = GetGPUHandle( m_fp_backbuffer.srv );
		m_pipeline.SetRes( hdr_buffer );

		SSAmbientLightingStorage ambient_buffer;
		ambient_buffer.resource = m_ambient_lighting.texture_gpu.Get();
		ambient_buffer.rtv = m_ambient_lighting.rtv->HandleCPU();
		ambient_buffer.srv = GetGPUHandle( m_ambient_lighting.srv );
		m_pipeline.SetRes( ambient_buffer );

		SSNormalStorage normal_buffer;
		normal_buffer.resource = m_normals.texture_gpu.Get();
		normal_buffer.rtv = m_normals.rtv->HandleCPU();
		normal_buffer.srv = GetGPUHandle( m_normals.srv );
		m_pipeline.SetRes( normal_buffer );

		DepthStorage depth_buffer;
		depth_buffer.dsv = DepthStencilView();
		depth_buffer.srv = DescriptorTables().GetTable( m_depth_buffer_srv )->gpu_handle;
		m_pipeline.SetRes( depth_buffer );

		ScreenConstants screen;
		screen.viewport = m_screen_viewport;
		screen.scissor_rect = m_scissor_rect;
		m_pipeline.SetRes( screen );

		SceneContext scene_ctx;
		scene_ctx.scene = &m_scene;
		m_pipeline.SetRes( scene_ctx );

		SSAOStorage ssao_texture;
		ssao_texture.resource = m_ssao.texture_gpu.Get();
		ssao_texture.rtv = m_ssao.rtv->HandleCPU();
		ssao_texture.srv = GetGPUHandle( m_ssao.srv );
		m_pipeline.SetRes( ssao_texture );

		TonemapNodeSettings tm_settings;
		tm_settings.data.blend_luminance = m_tonemap_settings.blend_luminance;
		tm_settings.data.lower_luminance_bound = m_tonemap_settings.min_luminance;
		tm_settings.data.upper_luminance_bound = m_tonemap_settings.max_luminance;
		m_pipeline.SetRes( tm_settings );

		HBAOSettings hbao_settings;
		hbao_settings.data = m_hbao_settings;
		m_pipeline.SetRes( hbao_settings );

		BackbufferStorage backbuffer;
		backbuffer.resource = CurrentBackBuffer();
		backbuffer.rtv = CurrentBackBufferView();
		m_pipeline.SetRes( backbuffer );

		ImGuiFontHeap font_srv_heap;
		font_srv_heap.heap = m_srv_ui_heap->GetInterface();
		m_pipeline.SetRes( font_srv_heap );
	}

	m_pipeline.Run( *m_cmd_list.Get() );

	rtv_barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition( CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT );
	rtv_barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition( m_depth_stencil_buffer.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COMMON );
	rtv_barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition( m_scene.lights["sun"].shadow_map->texture_gpu.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON );

	m_cmd_list->ResourceBarrier( 3, rtv_barriers );

	ThrowIfFailed( m_cmd_list->Close() );

	ID3D12CommandList* cmd_lists[] = { m_cmd_list.Get() };
	m_graphics_queue->GetCmdQueue()->ExecuteCommandLists( _countof( cmd_lists ), cmd_lists );

	EndFrame();	
}

void Renderer::NewGUIFrame()
{
	ImGui_ImplWin32_NewFrame();
}

void Renderer::Resize( size_t new_width, size_t new_height )
{
	RecreateSwapChainAndDepthBuffers( new_width, new_height );

	RecreatePrevFrameTexture( false );
}

void Renderer::CreateDevice()
{

#if defined(DEBUG) || defined(_DEBUG) 
	// Enable the D3D12 debug layer.
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed( D3D12GetDebugInterface( IID_PPV_ARGS( &debugController ) ) );
		debugController->EnableDebugLayer();
	}
#endif

	ThrowIfFailed( CreateDXGIFactory1( IID_PPV_ARGS( &m_dxgi_factory ) ) );

	// Try to create hardware device.
	HRESULT hardware_result;
	hardware_result = D3D12CreateDevice(
		nullptr,             // default adapter
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS( &m_d3d_device ) );


	// Fallback to WARP device.
	if ( FAILED( hardware_result ) )
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed( m_dxgi_factory->EnumWarpAdapter( IID_PPV_ARGS( &pWarpAdapter ) ) );

		ThrowIfFailed( D3D12CreateDevice(
			pWarpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS( &m_d3d_device ) ) );
	}
}

void Renderer::CreateBaseCommandObjects()
{
	ThrowIfFailed( m_d3d_device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS( m_direct_cmd_allocator.GetAddressOf() ) ) );

	ThrowIfFailed( m_d3d_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_direct_cmd_allocator.Get(), // Associated command allocator
		nullptr,                   // Initial PipelineStateObject
		IID_PPV_ARGS( m_cmd_list.GetAddressOf() ) ) );

	// Start off in a closed state.  This is because the first time we refer 
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
	m_cmd_list->Close();
}

void Renderer::CreateSwapChain()
{
	m_swap_chain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = UINT( m_client_width );
	sd.BufferDesc.Height = UINT( m_client_height );
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = m_back_buffer_format;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = SwapChainBufferCount;
	sd.OutputWindow = m_main_hwnd;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// Note: Swap chain uses queue to perform flush.
	ThrowIfFailed( m_dxgi_factory->CreateSwapChain(
		m_graphics_queue->GetCmdQueue(),
		&sd,
		m_swap_chain.GetAddressOf() ) );
}

void Renderer::RecreateSwapChainAndDepthBuffers( size_t new_width, size_t new_height )
{
	assert( m_swap_chain );
	assert( m_direct_cmd_allocator );

	// Flush before changing any resources.
	m_graphics_queue->Flush();

	ImGui_ImplDX12_InvalidateDeviceObjects();

	m_client_width = new_width;
	m_client_height = new_height;

	ThrowIfFailed( m_cmd_list->Reset( m_direct_cmd_allocator.Get(), nullptr ) );

	// Release the previous resources we will be recreating.
	for ( int i = 0; i < SwapChainBufferCount; ++i )
		m_swap_chain_buffer[i].Reset();
	m_depth_stencil_buffer.Reset();

	// Resize the swap chain.
	ThrowIfFailed( m_swap_chain->ResizeBuffers(
		SwapChainBufferCount,
		UINT( m_client_width ), UINT( m_client_height ),
		m_back_buffer_format,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH ) );

	m_curr_back_buff = 0;

	for ( size_t i = 0; i < SwapChainBufferCount; ++i )
	{
		ThrowIfFailed( m_swap_chain->GetBuffer( UINT( i ), IID_PPV_ARGS( &m_swap_chain_buffer[i] ) ) );

		m_back_buffer_rtv[i] = std::nullopt;
		m_back_buffer_rtv[i].emplace( std::move( m_rtv_heap->AllocateDescriptor() ) );
		m_d3d_device->CreateRenderTargetView( m_swap_chain_buffer[i].Get(), nullptr, m_back_buffer_rtv[i]->HandleCPU() );
	}

	// Create the depth/stencil buffer and view.
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = m_client_width;
	depthStencilDesc.Height = UINT( m_client_height );
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;

	depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;

	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = m_depth_stencil_format;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	ThrowIfFailed( m_d3d_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS( m_depth_stencil_buffer.GetAddressOf() ) ) );

	// Create descriptor to mip level 0 of entire resource using the format of the resource.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = m_depth_stencil_format;
	dsvDesc.Texture2D.MipSlice = 0;

	m_back_buffer_dsv.reset();
	m_back_buffer_dsv.emplace( std::move( m_dsv_heap->AllocateDescriptor() ) );
	m_d3d_device->CreateDepthStencilView( m_depth_stencil_buffer.Get(), &dsvDesc, m_back_buffer_dsv->HandleCPU() );

	// Transition the resource from its initial state to be used as a depth buffer.
	m_cmd_list->ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( m_depth_stencil_buffer.Get(),
																		   D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE ) );

   // Execute the resize commands.
	ThrowIfFailed( m_cmd_list->Close() );
	ID3D12CommandList* cmdsLists[] = { m_cmd_list.Get() };
	m_graphics_queue->GetCmdQueue()->ExecuteCommandLists( _countof( cmdsLists ), cmdsLists );

	// Wait until resize is complete.
	m_graphics_queue->Flush();

	// Update the viewport transform to cover the client area.
	m_screen_viewport.TopLeftX = 0;
	m_screen_viewport.TopLeftY = 0;
	m_screen_viewport.Width = static_cast<float>( m_client_width );
	m_screen_viewport.Height = static_cast<float>( m_client_height );
	m_screen_viewport.MinDepth = 0.0f;
	m_screen_viewport.MaxDepth = 1.0f;

	m_scissor_rect = { 0, 0, LONG( m_client_width ), LONG( m_client_height ) };

	ImGui_ImplDX12_CreateDeviceObjects();
}

void Renderer::BuildRtvAndDsvDescriptorHeaps()
{
	m_rtv_heap = std::make_unique<StagingDescriptorHeap>( D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_d3d_device );
	m_dsv_heap = std::make_unique<StagingDescriptorHeap>( D3D12_DESCRIPTOR_HEAP_TYPE_DSV, m_d3d_device );
}

void Renderer::BuildUIDescriptorHeap( )
{
	// srv heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc{};
		srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		srv_heap_desc.NumDescriptors = 1 /*imgui font*/;

		ComPtr<ID3D12DescriptorHeap> srv_heap;
		ThrowIfFailed( m_d3d_device->CreateDescriptorHeap( &srv_heap_desc, IID_PPV_ARGS( &srv_heap ) ) );
		m_srv_ui_heap = std::make_unique<DescriptorHeap>( std::move( srv_heap ), m_cbv_srv_uav_size );
	}
}

void Renderer::RecreatePrevFrameTexture( bool create_tables )
{

	auto recreate_tex = [&]( auto& tex, DXGI_FORMAT texture_format )
	{
		tex.texture_gpu = nullptr;

		CD3DX12_RESOURCE_DESC tex_desc( CurrentBackBuffer()->GetDesc() );
		tex_desc.Format = texture_format;
		D3D12_CLEAR_VALUE opt_clear;
		opt_clear.Color[0] = 0;
		opt_clear.Color[1] = 0;
		opt_clear.Color[2] = 0;
		opt_clear.Color[3] = 0;
		opt_clear.Format = texture_format;
		opt_clear.DepthStencil.Depth = 1.0f;
		opt_clear.DepthStencil.Stencil = 0;
		ThrowIfFailed( m_d3d_device->CreateCommittedResource( &CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ), D3D12_HEAP_FLAG_NONE,
																&tex_desc, D3D12_RESOURCE_STATE_COMMON,
																&opt_clear, IID_PPV_ARGS( &tex.texture_gpu ) ) );


		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
		{
			srv_desc.Format = tex_desc.Format;
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = 1;
		}
		if ( create_tables )
			tex.srv = DescriptorTables().AllocateTable( 1 );

		m_d3d_device->CreateShaderResourceView( tex.texture_gpu.Get(), &srv_desc, *DescriptorTables().ModifyTable( tex.srv ) );
	};

	recreate_tex( m_prev_frame_texture, DXGI_FORMAT_R16G16B16A16_FLOAT );
	recreate_tex( m_jittered_frame_texture, DXGI_FORMAT_R16G16B16A16_FLOAT );

	recreate_tex( m_fp_backbuffer, DXGI_FORMAT_R16G16B16A16_FLOAT );
	m_fp_backbuffer.rtv.reset();
	m_fp_backbuffer.rtv = std::make_unique<Descriptor>( std::move( m_rtv_heap->AllocateDescriptor() ) );
	m_d3d_device->CreateRenderTargetView( m_fp_backbuffer.texture_gpu.Get(), nullptr, m_fp_backbuffer.rtv->HandleCPU() );

	recreate_tex( m_ambient_lighting, DXGI_FORMAT_R16G16B16A16_FLOAT );
	m_ambient_lighting.rtv.reset();
	m_ambient_lighting.rtv = std::make_unique<Descriptor>( std::move( m_rtv_heap->AllocateDescriptor() ) );
	m_d3d_device->CreateRenderTargetView( m_ambient_lighting.texture_gpu.Get(), nullptr, m_ambient_lighting.rtv->HandleCPU() );

	recreate_tex( m_normals, DXGI_FORMAT_R16G16_FLOAT );
	m_normals.rtv.reset();
	m_normals.rtv = std::make_unique<Descriptor>( std::move( m_rtv_heap->AllocateDescriptor() ) );
	m_d3d_device->CreateRenderTargetView( m_normals.texture_gpu.Get(), nullptr, m_normals.rtv->HandleCPU() );

	recreate_tex( m_ssao, DXGI_FORMAT_R16_FLOAT );
	m_ssao.rtv.reset();
	m_ssao.rtv = std::make_unique<Descriptor>( std::move( m_rtv_heap->AllocateDescriptor() ) );
	m_d3d_device->CreateRenderTargetView( m_ssao.texture_gpu.Get(), nullptr, m_ssao.rtv->HandleCPU() );

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
	{
		srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;
	}
	m_d3d_device->CreateShaderResourceView( m_depth_stencil_buffer.Get(), &srv_desc, *DescriptorTables().ModifyTable( m_depth_buffer_srv ) );
}

void Renderer::InitImgui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	auto& imgui_io = ImGui::GetIO();
	ImGui_ImplWin32_Init( m_main_hwnd );
	BuildUIDescriptorHeap();
	m_ui_font_desc = std::make_unique<Descriptor>( std::move( m_srv_ui_heap->AllocateDescriptor() ) );
	ImGui_ImplDX12_Init( m_d3d_device.Get(), FrameResourceCount, m_back_buffer_format, m_ui_font_desc->HandleCPU(), m_ui_font_desc->HandleGPU() );
	ImGui::StyleColorsDark();
}

void Renderer::LoadAndBuildTextures( const ImportedScene& ext_scene, bool flush_per_texture )
{
	TextureID ph_albedo_id = m_scene_manager->GetScene().LoadStreamedTexture( "resources/textures/WoodCrate01.dds" );
	TextureID ph_normal_id = m_scene_manager->GetScene().LoadStreamedTexture( "resources/textures/default_deriv_normal.dds" );
	TextureID ph_specular_id = m_scene_manager->GetScene().LoadStreamedTexture( "resources/textures/default_spec.dds" );
	
	for ( size_t i = 0; i < ext_scene.textures.size(); ++i )
	{
		TextureID new_texture_id = m_scene_manager->GetScene().LoadStreamedTexture( ext_scene.textures[i] );
		if ( flush_per_texture )
		{
			m_scene_manager->UpdatePipelineBindings();
			m_scene_manager->FlushAllOperations();
			m_scene_manager->UpdatePipelineBindings();
			m_scene_manager->FlushAllOperations();
		}
		m_scene.textures.emplace( ext_scene.textures[i], new_texture_id );
	}
}

void Renderer::BuildGeometry( const ImportedScene& ext_scene )
{
	// static
	{
		m_geom_id = GetSceneView().LoadStaticMesh(
			"main",
			ext_scene.vertices,
			ext_scene.indices );

		for ( const auto& cpu_submesh : ext_scene.submeshes )
		{
			StaticSubmeshID submesh_id = GetSceneView().AddSubmesh( m_geom_id,
																	StaticSubmesh::Data{ uint32_t( cpu_submesh.nindices ),
																	                     uint32_t( cpu_submesh.index_offset ),
																	                     0 } );

			m_scene.static_geometry[cpu_submesh.name] = submesh_id;
		}
	}
}

void Renderer::BuildMaterials( const ImportedScene& ext_scene )
{
	m_placeholder_material = GetSceneView().AddMaterial( MaterialPBR::TextureIds{ m_scene.textures["placeholder_albedo"],
								m_scene.textures["placeholder_normal"],
								m_scene.textures["placeholder_specular"] }, XMFLOAT3( 0.03f, 0.03f, 0.03f ) );

	for ( int i = 0; i < ext_scene.materials.size(); ++i )
	{
		MaterialPBR::TextureIds textures;
		textures.base_color = m_scene.textures[ext_scene.textures[ext_scene.materials[i].second.base_color_tex_idx]];
		textures.normal = m_scene.textures[ext_scene.textures[ext_scene.materials[i].second.normal_tex_idx]];
		int spec_map_idx = ext_scene.materials[i].second.specular_tex_idx;
		if ( spec_map_idx < 0 )
			textures.specular = m_scene.textures["placeholder_specular"];
		else
			textures.specular = m_scene.textures[ext_scene.textures[spec_map_idx]];

		MaterialID new_material = GetSceneView().AddMaterial( textures, XMFLOAT3( 0.03f, 0.03f, 0.03f ) );

		auto& material = m_scene.materials.emplace( ext_scene.materials[i].first, new_material ).first->second;
	}
}

void Renderer::BuildRenderItems( const ImportedScene& ext_scene )
{
	m_scene.renderitems.reserve( ext_scene.submeshes.size() );

	for ( const auto& submesh : ext_scene.submeshes )
	{
		RenderItem item;

		item.geom = m_geom_id;
		const int material_idx = submesh.material_idx;
		std::string material_name;
		if ( material_idx < 0 )
			material_name = "placeholder";
		else
			material_name = ext_scene.materials[material_idx].first;

		item.material = m_scene.materials[material_name];

		item.tf_id = m_scene_manager->GetScene().AddTransform( submesh.transform );
		item.submesh = m_scene.static_geometry[submesh.name];
		m_scene.renderitems.push_back( item );
	}
}

void Renderer::BuildFrameResources( )
{
	for ( int i = 0; i < FrameResourceCount; ++i )
		m_frame_resources.emplace_back( m_d3d_device.Get(), 2, 1, 0 );
	m_cur_fr_idx = 0;
	m_cur_frame_resource = &m_frame_resources[m_cur_fr_idx];
}

void Renderer::BuildLights()
{
	auto& parallel_light = m_scene.lights.emplace( "sun", GPULight{ GPULight::Type::Parallel } ).first->second;
	LightConstants& data = parallel_light.data;

	// 304.5 wt/m^2
	data.strength = XMFLOAT3( 130.5f, 90.5f, 50.5f );
	data.dir = XMFLOAT3( 0.172f, -0.818f, -0.549f );
	XMFloat3Normalize( data.dir );
	parallel_light.shadow_map = nullptr;
	parallel_light.is_dynamic = true;


	auto& shadow_map = m_scene.shadow_maps["sun"];
	CreateShadowMap( shadow_map );
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc;
	{
		dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsv_desc.Format = m_depth_stencil_format;
		dsv_desc.Texture2D.MipSlice = 0;
		dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
	}
	shadow_map.dsv = std::make_unique<Descriptor>( std::move( m_dsv_heap->AllocateDescriptor() ) );
	m_d3d_device->CreateDepthStencilView( shadow_map.texture_gpu.Get(), &dsv_desc, shadow_map.dsv->HandleCPU() );

	parallel_light.shadow_map = &shadow_map;
}

void Renderer::BuildConstantBuffers()
{
	for ( int i = 0; i < FrameResourceCount; ++i )
	{
		// pass cb
		m_frame_resources[i].pass_cb = std::make_unique<Utils::UploadBuffer<PassConstants>>( m_d3d_device.Get(), PassCount, true );
	}
}

void Renderer::BuildPasses()
{
	ForwardLightingPass::BuildData( BuildStaticSamplers(), DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16_FLOAT, m_depth_stencil_format, *m_d3d_device.Get(),
									m_forward_pso_main, m_forward_pso_wireframe, m_forward_root_signature );
	m_forward_pass = std::make_unique<ForwardLightingPass>( m_forward_pso_main.Get(), m_forward_pso_wireframe.Get(), m_forward_root_signature.Get() );


	DepthOnlyPass::BuildData( m_depth_stencil_format, 5000, true, *m_d3d_device.Get(),
							  m_do_pso, m_do_root_signature );
	m_shadow_pass = std::make_unique<DepthOnlyPass>( m_do_pso.Get(), m_do_root_signature.Get() );

	{
		TemporalBlendPass::BuildData( DXGI_FORMAT_R16G16B16A16_FLOAT, *m_d3d_device.Get(), m_txaa_pso, m_txaa_root_signature );
		m_txaa_pass = std::make_unique<TemporalBlendPass>( m_txaa_pso.Get(), m_txaa_root_signature.Get() );

		ToneMappingPass::BuildData( m_back_buffer_format, *m_d3d_device.Get(), m_tonemap_pso, m_tonemap_root_signature );
		m_tonemap_pass = std::make_unique<ToneMappingPass>( m_tonemap_pso.Get(), m_tonemap_root_signature.Get() );
	}

	DepthOnlyPass::BuildData( m_depth_stencil_format, 0, true, *m_d3d_device.Get(),
							  m_z_prepass_pso, m_do_root_signature );
	m_depth_prepass = std::make_unique<DepthOnlyPass>( m_z_prepass_pso.Get(), m_do_root_signature.Get() );

	HBAOPass::BuildData( m_ssao.texture_gpu->GetDesc().Format, *m_d3d_device.Get(), m_hbao_pso, m_hbao_root_signature );
	m_hbao_pass = std::make_unique<HBAOPass>( m_hbao_pso.Get(), m_hbao_root_signature.Get() );

	m_pipeline.ConstructNode<DepthPrepassNode>( m_depth_prepass.get() );
	m_pipeline.ConstructNode<ForwardPassNode>( m_forward_pass.get() );
	m_pipeline.ConstructNode<HBAOGeneratorNode>( m_hbao_pass.get() );
	m_pipeline.ConstructNode<ShadowPassNode>( m_shadow_pass.get() );
	m_pipeline.ConstructNode<ToneMapPassNode>( m_tonemap_pass.get() );
	m_pipeline.ConstructNode<UIPassNode>();
	m_pipeline.Enable<DepthPrepassNode>();
	m_pipeline.Enable<ForwardPassNode>();
	m_pipeline.Enable<ShadowPassNode>();
	m_pipeline.Enable<ToneMapPassNode>();
	m_pipeline.Enable<UIPassNode>();
	m_pipeline.Enable<HBAOGeneratorNode>();

	if ( m_pipeline.IsRebuildNeeded() )
		m_pipeline.RebuildPipeline();

	ThrowIfFailed( m_d3d_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_frame_resources[0].cmd_list_allocs[1].Get(), // Associated command allocator
		nullptr, // Initial PipelineStateObject
		IID_PPV_ARGS( m_sm_cmd_lst.GetAddressOf() ) ) );
	m_sm_cmd_lst->Close();
}

void Renderer::CreateShadowMap( GPUTexture& texture )
{
	constexpr UINT width = 4096;
	constexpr UINT height = 4096;

	CD3DX12_RESOURCE_DESC tex_desc = CD3DX12_RESOURCE_DESC::Tex2D( DXGI_FORMAT_R32_TYPELESS, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL );
	D3D12_CLEAR_VALUE opt_clear;
	opt_clear.Format = m_depth_stencil_format;
	opt_clear.DepthStencil.Depth = 1.0f;
	opt_clear.DepthStencil.Stencil = 0;

	ThrowIfFailed( m_d3d_device->CreateCommittedResource( &CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ), D3D12_HEAP_FLAG_NONE,
														  &tex_desc, D3D12_RESOURCE_STATE_COMMON,
														  &opt_clear, IID_PPV_ARGS( &texture.texture_gpu ) ) );

	texture.srv = DescriptorTables().AllocateTable( 1 );

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
	{
		srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;
	}
	m_d3d_device->CreateShaderResourceView( texture.texture_gpu.Get(), &srv_desc, *DescriptorTables().ModifyTable( texture.srv ) );
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> Renderer::BuildStaticSamplers() const
{
	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP ); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP ); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP ); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP ); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8 );                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8 );                                // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC shadow(
		6,
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		0.0f,
		16,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE
	);
	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp, shadow };
}

void Renderer::EndFrame()
{
	// GPU timeline

	// swap buffers
	ThrowIfFailed( m_swap_chain->Present( 0, 0 ) );
	m_curr_back_buff = ( m_curr_back_buff + 1 ) % SwapChainBufferCount;

	m_cur_frame_resource->available_timestamp = m_graphics_queue->CreateTimestamp();

	// CPU timeline
	m_cur_fr_idx = ( m_cur_fr_idx + 1 ) % FrameResourceCount;
	m_cur_frame_resource = m_frame_resources.data() + m_cur_fr_idx;

	// wait for gpu to complete (i - 2)th frame;
	if ( m_cur_frame_resource->available_timestamp != 0 )
		m_graphics_queue->WaitForTimestamp( m_cur_frame_resource->available_timestamp );
}

ID3D12Resource* Renderer::CurrentBackBuffer()
{
	return m_swap_chain_buffer[m_curr_back_buff].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE Renderer::CurrentBackBufferView() const
{
	return m_back_buffer_rtv[m_curr_back_buff].value().HandleCPU();
}

D3D12_CPU_DESCRIPTOR_HANDLE Renderer::DepthStencilView() const
{
	return m_back_buffer_dsv.value().HandleCPU();
}
