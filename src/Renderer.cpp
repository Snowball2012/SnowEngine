// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "Renderer.h"

#include <imgui/imgui.h>
#include "imgui_impl/imgui_impl_win32.h"
#include "imgui_impl/imgui_impl_dx12.h"

#include "GeomGeneration.h"

#include "TemporalBlendPass.h"

#include <dxtk12/DDSTextureLoader.h>
#include <dxtk12/DirectXHelpers.h>

#include <d3dcompiler.h>
#include <d3d12sdklayers.h>

#include <future>

using namespace DirectX;

Renderer::Renderer( HWND main_hwnd, uint32_t screen_width, uint32_t screen_height )
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
	m_fp_backbuffer->RTV().reset();
	m_ambient_lighting->RTV().reset();
	m_normals->RTV().reset();
	m_ssao->RTV().reset();
	m_scene_manager.reset();
}

#include <dxtk12/ResourceUploadBatch.h>
void Renderer::Init()
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
	BuildRtvAndDsvDescriptorHeaps();

	m_scene_manager = std::make_unique<SceneManager>( m_d3d_device, m_dsv_heap.get(), FrameResourceCount, m_copy_queue.get() );
	m_forward_cb_provider = std::make_unique<ForwardCBProvider>( *m_d3d_device.Get(), FrameResourceCount );

	RecreateSwapChainAndDepthBuffers( m_client_width, m_client_height );
	CreateTransientTextures();

	BuildFrameResources();

	BuildPasses();

	m_pssm.SetSplitsNum( MAX_CASCADE_SIZE );
}


void Renderer::Draw( const Context& ctx )
{
	m_framegraph.ClearResources();

	if ( m_framegraph.IsRebuildNeeded() )
		m_framegraph.Rebuild();

	CameraID scene_camera;
	if ( ! ( m_frustrum_cull_camera_id == CameraID::nullid ) )
		scene_camera = m_frustrum_cull_camera_id;
	else
		scene_camera = m_main_camera_id;

	m_scene_manager->UpdateFramegraphBindings( scene_camera, PSSM(), m_screen_viewport );

	const Camera* main_camera = GetScene().GetCamera( m_main_camera_id );
	if ( ! main_camera )
		throw SnowEngineException( "no main camera" );
	m_forward_cb_provider->Update( main_camera->GetData(), PSSM(), GetScene().GetROScene().LightSpan() );

	m_scene_manager->BindToFramegraph( m_framegraph, *m_forward_cb_provider );

	// Reuse memory associated with command recording
	// We can only reset when the associated command lists have finished execution on GPU
	for ( auto& allocator : m_cur_frame_resource->cmd_list_allocs )
		ThrowIfFailed( allocator->Reset() );

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList. Reusing the command list reuses memory
	ThrowIfFailed( m_cmd_list->Reset( m_cur_frame_resource->cmd_list_allocs[0].Get(), nullptr ) );

	CD3DX12_RESOURCE_BARRIER rtv_barriers[10];
	rtv_barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition( CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET );
	rtv_barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition( m_fp_backbuffer->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET );
	rtv_barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition( m_ambient_lighting->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET );
	rtv_barriers[3] = CD3DX12_RESOURCE_BARRIER::Transition( m_normals->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET );
	rtv_barriers[4] = CD3DX12_RESOURCE_BARRIER::Transition( m_ssao->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET );
	rtv_barriers[5] = CD3DX12_RESOURCE_BARRIER::Transition( m_ssao_blurred->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
	rtv_barriers[6] = CD3DX12_RESOURCE_BARRIER::Transition( m_ssao_blurred_transposed->Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
	rtv_barriers[7] = CD3DX12_RESOURCE_BARRIER::Transition( m_depth_stencil_buffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	auto& sm_storage = m_framegraph.GetRes<ShadowMaps>();
	if ( ! sm_storage )
		throw SnowEngineException( "missing resource" );
	rtv_barriers[8] = CD3DX12_RESOURCE_BARRIER::Transition( sm_storage->res, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	auto& pssm_storage = m_framegraph.GetRes<ShadowCascade>();
	if ( ! pssm_storage )
		throw SnowEngineException( "missing resource" );

	rtv_barriers[9] = CD3DX12_RESOURCE_BARRIER::Transition( pssm_storage->res, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE );
	
	m_cmd_list->ResourceBarrier( 10, rtv_barriers );
	ID3D12DescriptorHeap* heaps[] = { m_scene_manager->GetDescriptorTables().CurrentGPUHeap().Get() };
	m_cmd_list->SetDescriptorHeaps( 1, heaps );

	{
		BindSkybox( main_camera->GetSkybox() );

		ForwardPassCB pass_cb{ m_forward_cb_provider->GetCBPointer() };
		m_framegraph.SetRes( pass_cb );

		HDRBuffer hdr_buffer;
		hdr_buffer.res = m_fp_backbuffer->Resource();
		hdr_buffer.rtv = m_fp_backbuffer->RTV()->HandleCPU();
		hdr_buffer.srv = GetGPUHandle( m_fp_backbuffer->SRV() );
		m_framegraph.SetRes( hdr_buffer );

		AmbientBuffer ambient_buffer;
		ambient_buffer.res = m_ambient_lighting->Resource();
		ambient_buffer.rtv = m_ambient_lighting->RTV()->HandleCPU();
		ambient_buffer.srv = GetGPUHandle( m_ambient_lighting->SRV() );
		m_framegraph.SetRes( ambient_buffer );

		NormalBuffer normal_buffer;
		normal_buffer.res = m_normals->Resource();
		normal_buffer.rtv = m_normals->RTV()->HandleCPU();
		normal_buffer.srv = GetGPUHandle( m_normals->SRV() );
		m_framegraph.SetRes( normal_buffer );

		DepthStencilBuffer depth_buffer;
		depth_buffer.dsv = DepthStencilView();
		depth_buffer.srv = DescriptorTables().GetTable( m_depth_buffer_srv )->gpu_handle;
		m_framegraph.SetRes( depth_buffer );

		ScreenConstants screen;
		screen.viewport = m_screen_viewport;
		screen.scissor_rect = m_scissor_rect;
		m_framegraph.SetRes( screen );

		SSAOBuffer_Noisy ssao_texture;
		ssao_texture.res = m_ssao->Resource();
		ssao_texture.rtv = m_ssao->RTV()->HandleCPU();
		ssao_texture.srv = GetGPUHandle( m_ssao->SRV() );
		m_framegraph.SetRes( ssao_texture );

		SSAOTexture_Blurred ssao_blurred_texture;
		ssao_blurred_texture.res = m_ssao_blurred->Resource();
		ssao_blurred_texture.uav = GetGPUHandle( m_ssao_blurred->UAV() );
		ssao_blurred_texture.srv = GetGPUHandle( m_ssao_blurred->SRV() );
		m_framegraph.SetRes( ssao_blurred_texture );


		SSAOTexture_Transposed ssao_blurred_texture_horizontal;
		ssao_blurred_texture_horizontal.res = m_ssao_blurred_transposed->Resource();
		ssao_blurred_texture_horizontal.uav = GetGPUHandle( m_ssao_blurred_transposed->UAV() );
		ssao_blurred_texture_horizontal.srv = GetGPUHandle( m_ssao_blurred_transposed->SRV() );
		m_framegraph.SetRes( ssao_blurred_texture_horizontal );


		TonemapNodeSettings tm_settings;
		tm_settings.data.blend_luminance = m_tonemap_settings.blend_luminance;
		tm_settings.data.lower_luminance_bound = m_tonemap_settings.min_luminance;
		tm_settings.data.upper_luminance_bound = m_tonemap_settings.max_luminance;
		m_framegraph.SetRes( tm_settings );

		HBAOSettings hbao_settings;
		hbao_settings.data = m_hbao_settings;
		m_framegraph.SetRes( hbao_settings );

		SDRBuffer backbuffer;
		backbuffer.resource = CurrentBackBuffer();
		backbuffer.rtv = CurrentBackBufferView();
		m_framegraph.SetRes( backbuffer );

		ImGuiFontHeap font_srv_heap;
		font_srv_heap.heap = m_srv_ui_heap->GetInterface();
		m_framegraph.SetRes( font_srv_heap );
	}

	m_framegraph.Run( *m_cmd_list.Get() );

	rtv_barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition( CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT );
	rtv_barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition( m_depth_stencil_buffer.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COMMON );
	rtv_barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition( sm_storage->res, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON );
	rtv_barriers[3] = CD3DX12_RESOURCE_BARRIER::Transition( pssm_storage->res, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON );

	m_cmd_list->ResourceBarrier( 4, rtv_barriers );

	ThrowIfFailed( m_cmd_list->Close() );

	ID3D12CommandList* cmd_lists[] = { m_cmd_list.Get() };
	m_graphics_queue->GetCmdQueue()->ExecuteCommandLists( _countof( cmd_lists ), cmd_lists );

	EndFrame();	
}

void Renderer::NewGUIFrame()
{
	ImGui_ImplWin32_NewFrame();
}

void Renderer::Resize( uint32_t new_width, uint32_t new_height )
{
	RecreateSwapChainAndDepthBuffers( new_width, new_height );

	ResizeTransientTextures( );
}

bool Renderer::SetMainCamera( CameraID id )
{
	if ( ! GetScene().GetROScene().AllCameras().has( id ) )
		return false;
	m_main_camera_id = id;
	return true;
}

bool Renderer::SetFrustrumCullCamera( CameraID id )
{
	if ( ! GetScene().GetROScene().AllCameras().has( id ) )
		return false;
	m_frustrum_cull_camera_id = id;
	return true;
}

Renderer::PerformanceStats Renderer::GetPerformanceStats() const noexcept
{
	return { m_scene_manager->GetTexStreamer().GetPerformanceStats() };
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
		nullptr,                   // Initial FramegraphStateObject
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

void Renderer::RecreateSwapChainAndDepthBuffers( uint32_t new_width, uint32_t new_height )
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
	optClear.DepthStencil.Depth = 0.0f;
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

void Renderer::CreateTransientTextures()
{
	// little hack here, create 1x1 textures and resize them right after
	
	auto create_tex = [&]( std::unique_ptr<DynamicTexture>& tex, DXGI_FORMAT texture_format,
						   bool create_srv_table, bool create_uav_table, bool create_rtv_desc )
	{
		CD3DX12_RESOURCE_DESC desc( CD3DX12_RESOURCE_DESC::Tex2D( texture_format,
																  /*width=*/1, /*height=*/1,
																  /*depth=*/1, /*mip_levels=*/1 ) );

		D3D12_CLEAR_VALUE clear_value;
		clear_value.Color[0] = 0;
		clear_value.Color[1] = 0;
		clear_value.Color[2] = 0;
		clear_value.Color[3] = 0;
		clear_value.Format = texture_format;

		if ( create_uav_table )
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		if ( create_rtv_desc )
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		// create resource (maybe create it in DynamicTexture?)
		{
			const D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;

			const auto* opt_clear_ptr = create_rtv_desc ? &clear_value : nullptr;
			ComPtr<ID3D12Resource> res;
			ThrowIfFailed( m_d3d_device->CreateCommittedResource( &CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ), D3D12_HEAP_FLAG_NONE,
																  &desc, initial_state,
																  opt_clear_ptr,
																  IID_PPV_ARGS( res.GetAddressOf() ) ) );

			tex = std::make_unique<DynamicTexture>( std::move( res ), m_d3d_device.Get(), initial_state, opt_clear_ptr );
		}

		if ( create_srv_table )
			tex->SRV() = DescriptorTables().AllocateTable( 1 );
		if ( create_uav_table )
			tex->UAV() = DescriptorTables().AllocateTable( 1 );
		if ( create_rtv_desc )
			tex->RTV() = std::make_unique<Descriptor>( std::move( m_rtv_heap->AllocateDescriptor() ) );
	};

	create_tex( m_fp_backbuffer, DXGI_FORMAT_R16G16B16A16_FLOAT, true, false, true );
	create_tex( m_ambient_lighting, DXGI_FORMAT_R16G16B16A16_FLOAT, true, false, true );
	create_tex( m_normals, DXGI_FORMAT_R16G16_FLOAT, true, false, true );
	create_tex( m_ssao, DXGI_FORMAT_R16_FLOAT, true, false, true );
	create_tex( m_ssao_blurred, DXGI_FORMAT_R16_FLOAT, true, true, false );
	create_tex( m_ssao_blurred_transposed, DXGI_FORMAT_R16_FLOAT, true, true, false );

	m_depth_buffer_srv = DescriptorTables().AllocateTable( 1 );

	ResizeTransientTextures();
}

void Renderer::ResizeTransientTextures( )
{
	auto resize_tex = [&]( auto& tex, uint32_t width, uint32_t height, bool make_srv, bool make_uav, bool make_rtv )
	{
		tex.Resize( width, height );
		if ( make_srv )
			m_d3d_device->CreateShaderResourceView( tex.Resource(), nullptr, *DescriptorTables().ModifyTable( tex.SRV() ) );
		if ( make_uav )
			m_d3d_device->CreateUnorderedAccessView( tex.Resource(), nullptr, nullptr, *DescriptorTables().ModifyTable( tex.UAV() ) );
		if ( make_rtv )
			m_d3d_device->CreateRenderTargetView( tex.Resource(), nullptr, tex.RTV()->HandleCPU() );
	};

	resize_tex( *m_fp_backbuffer, m_client_width, m_client_height, true, false, true );
	resize_tex( *m_ambient_lighting, m_client_width, m_client_height, true, false, true );
	resize_tex( *m_normals, m_client_width, m_client_height, true, false, true );
	resize_tex( *m_ssao, m_client_width / 2, m_client_height / 2, true, false, true );
	resize_tex( *m_ssao_blurred, m_client_width, m_client_height, true, true, false );
	resize_tex( *m_ssao_blurred_transposed, m_client_height, m_client_width, true, true, false );

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
	{
		srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;
	}
	m_d3d_device->CreateShaderResourceView( m_depth_stencil_buffer.Get(), &srv_desc, DescriptorTables().ModifyTable( m_depth_buffer_srv ).value() );
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

void Renderer::BuildFrameResources( )
{
	for ( int i = 0; i < FrameResourceCount; ++i )
		m_frame_resources.emplace_back( m_d3d_device.Get(), 2 );
	m_cur_fr_idx = 0;
	m_cur_frame_resource = &m_frame_resources[m_cur_fr_idx];
}

void Renderer::BuildPasses()
{
	m_framegraph.ConstructAndEnableNode<DepthPrepassNode>( m_depth_stencil_format, *m_d3d_device.Get() );

	m_framegraph.ConstructAndEnableNode<ShadowPassNode>( m_depth_stencil_format, 5000, *m_d3d_device.Get() );
	m_framegraph.ConstructAndEnableNode<PSSMNode>( m_depth_stencil_format, 5000, *m_d3d_device.Get() );

	m_framegraph.ConstructAndEnableNode<ForwardPassNode>( DXGI_FORMAT_R16G16B16A16_FLOAT, // rendertarget
														DXGI_FORMAT_R16G16B16A16_FLOAT, // ambient lighting
														DXGI_FORMAT_R16G16_FLOAT, // normals
														m_depth_stencil_format, *m_d3d_device.Get() );

	m_framegraph.ConstructAndEnableNode<SkyboxNode>( DXGI_FORMAT_R16G16B16A16_FLOAT, m_depth_stencil_format, *m_d3d_device.Get() );

	m_framegraph.ConstructAndEnableNode<HBAONode>( m_ssao->Resource()->GetDesc().Format, *m_d3d_device.Get() );
	m_framegraph.ConstructAndEnableNode<BlurSSAONode>( *m_d3d_device.Get() );
	m_framegraph.ConstructAndEnableNode<ToneMapNode>( m_back_buffer_format, *m_d3d_device.Get() );
	m_framegraph.ConstructAndEnableNode<UIPassNode>();


	if ( m_framegraph.IsRebuildNeeded() )
		m_framegraph.Rebuild();
}

void Renderer::BindSkybox( EnvMapID skybox_id )
{
	assert( m_scene_manager->GetScene().GetROScene().AllEnviromentMaps().has( skybox_id ) );

	const auto& scene = m_scene_manager->GetScene().GetROScene();

	const EnviromentMap& skybox = scene.AllEnviromentMaps()[skybox_id];

	Skybox framegraph_res;
	const TextureID* cylindrical_tex_id = std::get_if<TextureID>( &skybox.GetMap() );
	if ( ! cylindrical_tex_id )
		NOTIMPL; // cubemap?

	const Texture* tex = scene.AllTextures().try_get( *cylindrical_tex_id );
	if ( ! tex )
		throw SnowEngineException( "skybox does not have a texture attached" );

	if ( tex->IsLoaded() )
		framegraph_res.srv = skybox.GetSRV();
	else
		framegraph_res.srv.ptr = 0;

	const ObjectTransform* tf = scene.AllTransforms().try_get( skybox.GetTransform() );
	if ( ! tf )
		throw SnowEngineException( "skybox does not have a transform attached" );

	framegraph_res.tf_cbv = tf->GPUView();

	framegraph_res.radiance_factor = skybox.GetRadianceFactor();

	m_framegraph.SetRes( framegraph_res );
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
