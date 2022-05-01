/ This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "OldRenderer.h"

#include "Renderer.h"

#include "GeomGeneration.h"

#include "TemporalBlendPass.h"

#include "GPUResourceHolder.h"

#include <utils/CGUtils.h>

#include <dxtk12/DDSTextureLoader.h>
#include <dxtk12/DirectXHelpers.h>
#include <dxtk12/ResourceUploadBatch.h>

#include <d3dcompiler.h>
#include <d3d12sdklayers.h>

#include <imgui/imgui.h>
#include "imgui_impl/imgui_impl_win32.h"
#include "imgui_impl/imgui_impl_dx12.h"

#include <future>

using namespace DirectX;

OldRenderer::OldRenderer( HWND main_hwnd, uint32_t screen_width, uint32_t screen_height )
    : m_main_hwnd( main_hwnd ), m_client_width( screen_width ), m_client_height( screen_height )
{
}

OldRenderer::~OldRenderer()
{
    for ( auto& desc : m_back_buffer_rtv )
        desc.reset();

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    m_ui_font_desc.reset();
    m_renderer.reset();
    m_scene_manager.reset();
}


void OldRenderer::Init()
{
    CreateDevice();
    CreateSwapChain();

    InitImgui();
    BuildRtvDescriptorHeaps();

    m_scene_manager = std::make_unique<SceneManager>( m_gpu_device.get(), FrameResourceCount, m_gpu_device->GetCopyQueue(), m_gpu_device->GetGraphicsQueue() );

    RecreateSwapChain( m_client_width, m_client_height );

    BuildFrameResources();

    Renderer::DeviceContext device_ctx;
    device_ctx.device = m_gpu_device.get();
    device_ctx.srv_cbv_uav_tables = &DescriptorTables();
    m_renderer = std::make_unique<Renderer>( Renderer::Create( device_ctx, m_client_width, m_client_height ) );

    m_renderer->GetPSSM().SetSplitsNum( MAX_CASCADE_SIZE );

    m_ibl_table = DescriptorTables().AllocateTable( 2 );
}


void OldRenderer::Draw()
{
    OPTICK_EVENT();
    // Update rendergraph
    World::Entity scene_camera = m_main_camera;

    const Scene& scene = GetScene().GetROScene();

    {
        D3D12_CPU_DESCRIPTOR_HANDLE desc_table = *DescriptorTables().ModifyTable( m_ibl_table );
        const size_t descriptor_size = m_gpu_device->GetDescriptorSizes().cbv_srv_uav;
        if ( auto* tex = scene.AllCubemaps().try_get( m_irradiance_map ) )
        {
            if ( tex->IsLoaded() )
            {
                m_gpu_device->GetNativeDevice()->CopyDescriptorsSimple( 1, CD3DX12_CPU_DESCRIPTOR_HANDLE( desc_table, 0, UINT( descriptor_size ) ),
                                                     tex->StagingSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
            }
        }
        if ( auto* tex = scene.AllCubemaps().try_get( m_reflection_probe ) )
        {
            if ( tex->IsLoaded() )
            {
                m_gpu_device->GetNativeDevice()->CopyDescriptorsSimple( 1, CD3DX12_CPU_DESCRIPTOR_HANDLE( desc_table, 1, UINT( descriptor_size ) ),
                                                     tex->StagingSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
            }
        }
    }

    m_scene_manager->UpdateFramegraphBindings( scene_camera, m_renderer->GetPSSM(), m_screen_viewport );

    // Draw scene
    const Camera* main_camera = GetScene().GetWorld().GetComponent<Camera>( scene_camera );

	if ( ! SE_ENSURE( main_camera ) )
        throw SnowEngineException( "no main camera" );

    std::vector<CommandList> lists_to_execute;

    lists_to_execute.emplace_back( m_gpu_device->GetGraphicsQueue()->CreateCommandList() );
    lists_to_execute.back().Reset();

    IGraphicsCommandList* cmd_list = lists_to_execute.back().GetInterface();

    CD3DX12_RESOURCE_BARRIER barriers[1];
    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition( CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET );
    
    cmd_list->ResourceBarrier( 1, barriers );
    ID3D12DescriptorHeap* heaps[] = { m_scene_manager->GetDescriptorTables().CurrentGPUHeap().Get() };
    cmd_list->SetDescriptorHeaps( 1, heaps );

    ThrowIfFailedH( cmd_list->Close() );
    
    constexpr UINT64 heap_block_size = 1024 * 1024;
    
    GPULinearAllocator allocator( m_gpu_device->GetNativeDevice(), CD3DX12_HEAP_DESC( heap_block_size, D3D12_HEAP_TYPE_UPLOAD, 0, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS ) );

    RenderTask task = m_renderer->CreateTask( main_camera->GetData(), GetScene().GetAllLights(),
                                              RenderMode::FullTonemapped, allocator );

    Renderer::SceneContext scene_ctx;
    scene_ctx.ibl_table = m_ibl_table;

    const EnvironmentMap* env_map = GetScene().GetEnviromentMap( main_camera->GetSkybox() );
    if ( env_map )
    {
        scene_ctx.skybox.cubemap_srv = env_map->GetSRV();
        scene_ctx.skybox.obj2world_mat = scene.AllTransforms()[env_map->GetTransform()].Obj2World();
        scene_ctx.skybox.radiance_factor = env_map->GetRadianceFactor();
    }
    else
    {
        m_renderer->SetSkybox( false );
    }

    RenderItemStorage render_lists = CreateRenderItems( task, DirectX::XMLoadFloat3( &main_camera->GetData().pos ) );

	auto opaque_list = m_renderer->CreateRenderitems( make_span( render_lists[0] ), allocator );
	task.AddOpaqueBatches( make_span( opaque_list.batches ) );

	std::vector<Renderer::RenderBatchList> shadow_lists;
	shadow_lists.resize( render_lists.size() - 1 );
	size_t list_idx = 0;
	size_t shadow_idx = 0;
	for ( const auto& shadow : task.GetShadowFrustums() )
	{
		size_t split_idx = 0;
		for ( const auto& split : shadow.frustum )
		{
			shadow_lists[list_idx] = m_renderer->CreateRenderitems( make_span( render_lists[list_idx + 1] ), allocator );
			task.AddShadowBatches( make_span( shadow_lists[list_idx].batches ), uint32_t( shadow_idx ), uint32_t( split_idx ) ); 

			list_idx++;
			split_idx++;
		}
		shadow_idx++;
	}

    Renderer::FrameContext frame_ctx;
    frame_ctx.render_target.resource = CurrentBackBuffer();
    frame_ctx.render_target.rtv = CurrentBackBufferView();
    frame_ctx.render_target.current_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
    frame_ctx.render_target.uav = DescriptorTableID::nullid;
    frame_ctx.render_target.viewport = m_screen_viewport;
    frame_ctx.render_target.scissor_rect = m_scissor_rect;
    frame_ctx.resources = &m_cur_frame_resource->second;

    frame_ctx.resources->AddAllocators( make_single_elem_span( allocator ) );
    frame_ctx.resources->AddResources( make_single_elem_span( opaque_list.per_obj_cb ) );
	for ( auto& shadow_list : shadow_lists )
		frame_ctx.resources->AddResources( make_single_elem_span( shadow_list.per_obj_cb ) );

    m_renderer->SetHBAOSettings( Renderer::HBAOSettings{ m_hbao_settings.max_r, m_hbao_settings.angle_bias, m_hbao_settings.nsamples_per_direction } );
    m_renderer->SetTonemapSettings( m_tonemap_settings );

    m_renderer->Draw( task, scene_ctx, frame_ctx, lists_to_execute );

    // Draw UI
    lists_to_execute.emplace_back( m_gpu_device->GetGraphicsQueue()->CreateCommandList() );
    lists_to_execute.back().Reset();
    cmd_list = lists_to_execute.back().GetInterface();
    ImGui_ImplDX12_NewFrame( cmd_list );
    heaps[0] = m_srv_ui_heap->GetInterface();
    cmd_list->OMSetRenderTargets( 1, &CurrentBackBufferView(), true, nullptr );
    cmd_list->SetDescriptorHeaps( 1, heaps );
    ImGui_ImplDX12_RenderDrawData( ImGui::GetDrawData() );

    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition( CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT );
    
    cmd_list->ResourceBarrier( 1, barriers );

    ThrowIfFailedH( cmd_list->Close() );

     m_gpu_device->GetGraphicsQueue()->SubmitLists( make_span( lists_to_execute ) );

    EndFrame( );	
}

void OldRenderer::NewGUIFrame()
{
    ImGui_ImplWin32_NewFrame();
}

void OldRenderer::Resize( uint32_t new_width, uint32_t new_height )
{
    m_gpu_device->Flush();
    RecreateSwapChain( new_width, new_height );
    m_renderer->SetInternalResolution( new_width, new_height );
}

ParallelSplitShadowMapping& OldRenderer::PSSM() noexcept { return m_renderer->GetPSSM(); }

bool OldRenderer::SetMainCamera( World::Entity camera )
{
	// Can accept nullid
	if ( camera.valid() )
		if ( ! SE_ENSURE( GetScene().GetWorld().GetComponent<Camera>( camera ) ) )
			return false;

    m_main_camera = camera;
    return true;
}

bool OldRenderer::SetFrustumCullCamera( World::Entity camera )
{
	// Can accept nullid
	if ( camera.valid() )
		if ( ! SE_ENSURE( GetScene().GetWorld().GetComponent<Camera>( camera ) ) )
			return false;

    m_frustum_cull_camera = camera;
    return true;
}

bool OldRenderer::SetIrradianceMap( CubemapID id )
{
    if ( ! GetScene().GetROScene().AllCubemaps().has( id ) )
        return false;
    m_irradiance_map = id;
    return true;
}

bool OldRenderer::SetReflectionProbe( CubemapID id )
{
    if ( ! GetScene().GetROScene().AllCubemaps().has( id ) )
        return false;
    m_reflection_probe = id;
    return true;
}

void OldRenderer::SetSkybox( bool enable )
{
    m_renderer->SetSkybox( enable );
}

OldRenderer::PerformanceStats OldRenderer::GetPerformanceStats() const noexcept
{
    return {
        m_scene_manager->GetTexStreamer().GetPerformanceStats(),
        m_num_renderitems_total,
        m_num_renderitems_to_draw
    };
}

void OldRenderer::CreateDevice()
{

#if defined(DEBUG) || defined(_DEBUG) 
    ComPtr<ID3D12Debug> spDebugController0;
    ComPtr<ID3D12Debug1> spDebugController1;
    ThrowIfFailedH( D3D12GetDebugInterface( IID_PPV_ARGS( &spDebugController0 ) ) );
    spDebugController0->EnableDebugLayer();
    ThrowIfFailedH( spDebugController0->QueryInterface( IID_PPV_ARGS( &spDebugController1 ) ) );
    //spDebugController1->SetEnableGPUBasedValidation( true );
#endif

    ThrowIfFailedH( CreateDXGIFactory1( IID_PPV_ARGS( &m_dxgi_factory ) ) );

    // Try to create hardware device.
    ComPtr<ID3D12Device> d3d_device;
    HRESULT hardware_result;
    hardware_result = D3D12CreateDevice(
        nullptr,             // default adapter
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS( &d3d_device ) );


    // Fallback to WARP device.
    if ( FAILED( hardware_result ) )
    {
        ComPtr<IDXGIAdapter> pWarpAdapter;
        ThrowIfFailedH( m_dxgi_factory->EnumWarpAdapter( IID_PPV_ARGS( &pWarpAdapter ) ) );

        ThrowIfFailedH( D3D12CreateDevice(
            pWarpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS( &d3d_device ) ) );
    }

    m_gpu_device = std::make_unique<GPUDevice>(std::move(d3d_device), false);
}


void OldRenderer::CreateSwapChain()
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
    ThrowIfFailedH( m_dxgi_factory->CreateSwapChain(
        m_gpu_device->GetGraphicsQueue()->GetNativeQueue(),
        &sd,
        m_swap_chain.GetAddressOf() ) );
}

void OldRenderer::RecreateSwapChain( uint32_t new_width, uint32_t new_height )
{
    assert( m_swap_chain );

    // Flush before changing any resources.
    m_gpu_device->Flush();

    ImGui_ImplDX12_InvalidateDeviceObjects();

    m_client_width = new_width;
    m_client_height = new_height;

    // Release the previous resources we will be recreating.
    for ( int i = 0; i < SwapChainBufferCount; ++i )
        m_swap_chain_buffer[i].Reset();

    // Resize the swap chain.
    ThrowIfFailedH( m_swap_chain->ResizeBuffers(
        SwapChainBufferCount,
        UINT( m_client_width ), UINT( m_client_height ),
        m_back_buffer_format,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH ) );

    m_curr_back_buff = 0;

    for ( size_t i = 0; i < SwapChainBufferCount; ++i )
    {
        ThrowIfFailedH( m_swap_chain->GetBuffer( UINT( i ), IID_PPV_ARGS( &m_swap_chain_buffer[i] ) ) );

        m_back_buffer_rtv[i] = std::nullopt;
        m_back_buffer_rtv[i].emplace( std::move( m_rtv_heap->AllocateDescriptor() ) );
        m_gpu_device->GetNativeDevice()->CreateRenderTargetView( m_swap_chain_buffer[i].Get(), nullptr, m_back_buffer_rtv[i]->HandleCPU() );
    }

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

void OldRenderer::BuildRtvDescriptorHeaps()
{
    m_rtv_heap = std::make_unique<StagingDescriptorHeap>( D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_gpu_device->GetNativeDevice() );
}

void OldRenderer::BuildUIDescriptorHeap( )
{
    // srv heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc{};
        srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        srv_heap_desc.NumDescriptors = 1 /*imgui font*/;

        ComPtr<ID3D12DescriptorHeap> srv_heap;
        ThrowIfFailedH( m_gpu_device->GetNativeDevice()->CreateDescriptorHeap( &srv_heap_desc, IID_PPV_ARGS( &srv_heap ) ) );
        m_srv_ui_heap = std::make_unique<DescriptorHeap>( std::move( srv_heap ), m_gpu_device->GetDescriptorSizes().cbv_srv_uav );
    }
}


void OldRenderer::InitImgui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& imgui_io = ImGui::GetIO();
    ImGui_ImplWin32_Init( m_main_hwnd );
    BuildUIDescriptorHeap();
    m_ui_font_desc = std::make_unique<Descriptor>( std::move( m_srv_ui_heap->AllocateDescriptor() ) );
    ImGui_ImplDX12_Init( m_gpu_device->GetNativeDevice(), FrameResourceCount, m_back_buffer_format, m_ui_font_desc->HandleCPU(), m_ui_font_desc->HandleGPU() );
    ImGui::StyleColorsDark();
}

void OldRenderer::BuildFrameResources( )
{
    m_frame_resources.resize( FrameResourceCount );
    m_cur_fr_idx = 0;
    m_cur_frame_resource = &m_frame_resources[m_cur_fr_idx];
}

void OldRenderer::EndFrame( )
{
    // GPU timeline

    m_cur_frame_resource->first = m_gpu_device->GetGraphicsQueue()->ExecuteSubmitted();
    // swap buffers
    OPTICK_GPU_FLIP(m_swap_chain.Get());
	OPTICK_CATEGORY("Present", Optick::Category::Wait);
    ThrowIfFailedH( m_swap_chain->Present( 0, 0 ) );
    m_curr_back_buff = ( m_curr_back_buff + 1 ) % SwapChainBufferCount;

    // CPU timeline
    m_cur_fr_idx = ( m_cur_fr_idx + 1 ) % FrameResourceCount;
    m_cur_frame_resource = m_frame_resources.data() + m_cur_fr_idx;

    // wait for gpu to complete (i - 2)th frame;
    if ( m_cur_frame_resource->first != 0 )
        m_gpu_device->GetGraphicsQueue()->WaitForTimestamp( m_cur_frame_resource->first );

    m_cur_frame_resource->second.Clear();
}

OldRenderer::RenderItemStorage OldRenderer::CreateRenderItems( const RenderTask& task, const DirectX::XMVECTOR& camera_pos )
{
    OPTICK_EVENT();
	
	const auto& main_frustum = task.GetMainPassFrustum();
    if ( main_frustum.type != RenderTask::Frustum::Type::Perspective )
        NOTIMPL;

	const span<const RenderTask::ShadowFrustum> shadow_frustums = task.GetShadowFrustums();
	
    const Scene& scene = GetScene().GetROScene();

	RenderItemStorage lists;
	{
		// count lists
		size_t nlists = 1; // main frustum
		for ( const auto& shadow_frustum : task.GetShadowFrustums() )
			nlists += shadow_frustum.frustum.size();
		lists.resize( nlists );
		// reserve mem. Probably too conservative
		const size_t nentities = scene.world.GetEntityCount();
		for ( auto& list : lists )
			list.reserve( nentities );
	}

	// prepare frustum culling data
    const DirectX::XMMATRIX& proj = main_frustum.proj;

    DirectX::BoundingFrustum main_bf( proj );
    if ( main_bf.Far < main_bf.Near ) // reversed z
        std::swap( main_bf.Far, main_bf.Near );

    const DirectX::XMMATRIX& view = main_frustum.view;

    DirectX::XMVECTOR det;
    main_bf.Transform( main_bf, DirectX::XMMatrixInverse( &det, view ) );

    m_num_renderitems_total = 0;
    m_num_renderitems_to_draw = 0;
	
	bc::small_vector<bool, MAX_CASCADE_SIZE> splits;

	auto& opaque_list = lists[0];

    for ( const auto& [entity, transform, drawable] : GetScene().GetWorld().CreateView<Transform, DrawableMesh>() )
    {
        if ( ! drawable.show )
            continue;

        const StaticSubmesh_Deprecated& submesh = scene.AllStaticSubmeshes()[drawable.mesh];
        const StaticMesh& geom = scene.AllStaticMeshes()[submesh.GetMesh()];
        if ( ! geom.IsLoaded() )
            continue;

        RenderItem item;
        item.ibv = geom.IndexBufferView();
        item.vbv = geom.VertexBufferView();

        const auto& submesh_draw_args = submesh.DrawArgs();

        item.index_count = submesh_draw_args.idx_cnt;
        item.index_offset = submesh_draw_args.start_index_loc;
        item.vertex_offset = submesh_draw_args.base_vertex_loc;

        const MaterialPBR& material = scene.AllMaterials()[drawable.material];

        item.material = &material;

        bool has_unloaded_texture = false;
        const auto& textures = material.Textures();
        for ( TextureID tex_id : { textures.base_color, textures.normal, textures.specular, textures.preintegrated_brdf } )
        {
            if ( ! scene.AllTextures()[tex_id].IsLoaded() )
            {
                has_unloaded_texture = true;
                break;
            }
        }

        if ( has_unloaded_texture )
            continue;

        DirectX::XMStoreFloat4x4( &item.local2world, transform.local2world );

        DirectX::BoundingOrientedBox item_box;
        DirectX::BoundingOrientedBox::CreateFromBoundingBox( item_box, submesh.Box() );

        item_box.Transform( item_box, transform.local2world );

        m_num_renderitems_total++;
        if ( item_box.Intersects( main_bf ) )
            opaque_list.push_back( item );

		// Shadow culling
		const DirectX::XMFLOAT3 center = item_box.Center;
		const float radius = std::sqrt( XMFloat3LenSquared( item_box.Extents ) );
		DirectX::XMVECTOR center_to_camera = DirectX::XMLoadFloat3(&center) - camera_pos;

		size_t list_idx = 1;
		for ( size_t shadow_idx = 0; shadow_idx < shadow_frustums.size(); ++shadow_idx )
		{
			const auto& shadow = shadow_frustums[shadow_idx];
			assert( !shadow.frustum.empty() );
			assert( shadow.frustum[0].type == RenderTask::Frustum::Type::Orthographic );
			assert( shadow.light != nullptr );

			splits.resize( shadow.split_positions.size() + 1 );
			for ( bool& in_split : splits )
				in_split = false;

			FindPSSMFrustumsForItem(
				center_to_camera, radius,
				XMLoadFloat3( &shadow.light->GetData().dir ), make_span( shadow.split_positions ),
				make_span( splits ) );

			for ( size_t split_idx = 0; split_idx < splits.size(); ++split_idx )
			{
				if ( splits[split_idx] )
					lists[list_idx + split_idx].push_back( item );
			}
			list_idx += splits.size();
		}
    }

	for ( auto& list : lists )
		boost::sort( list, []( const auto& lhs, const auto& rhs ) { return lhs.material < rhs.material; } );

    m_num_renderitems_to_draw = lists[0].size();

    return std::move( lists );
}

ID3D12Resource* OldRenderer::CurrentBackBuffer()
{
    return m_swap_chain_buffer[m_curr_back_buff].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE OldRenderer::CurrentBackBufferView() const
{
    return m_back_buffer_rtv[m_curr_back_buff].value().HandleCPU();
}
