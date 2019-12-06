// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "Renderer.h"


Renderer Renderer::Create( const DeviceContext& ctx, uint32_t width, uint32_t height )
{
    assert( ctx.device );

    StagingDescriptorHeap dsv_heap( D3D12_DESCRIPTOR_HEAP_TYPE_DSV, ctx.device );
    StagingDescriptorHeap rtv_heap( D3D12_DESCRIPTOR_HEAP_TYPE_RTV, ctx.device );
    ShadowProvider shadow_provider( ctx.device, ctx.srv_cbv_uav_tables );

    return Renderer( ctx, width, height,
                          std::move( dsv_heap ), std::move( rtv_heap ),
                          std::move( shadow_provider ) );
}


Renderer::~Renderer()
{
    m_depth_stencil_buffer.reset();
    m_hdr_backbuffer.reset();
    m_hdr_ambient.reset();
    m_normals.reset();
    m_ssao.reset();
}


Renderer::Renderer( const DeviceContext& ctx,
                    uint32_t width, uint32_t height,
                    StagingDescriptorHeap&& dsv_heap,
                    StagingDescriptorHeap&& rtv_heap,
                    ShadowProvider&& shadow_provider ) noexcept
    : m_dsv_heap( std::move( dsv_heap ) )
    , m_rtv_heap( std::move( rtv_heap ) )
    , m_shadow_provider( std::move( shadow_provider ) )
{
    assert( ctx.device );
    assert( ctx.srv_cbv_uav_tables );
    assert( width > 0 );
    assert( height > 0 );

    m_device = ctx.device;
    m_descriptor_tables = ctx.srv_cbv_uav_tables;

    m_resolution_width = width;
    m_resolution_height = height;
    CreateTransientResources();

    InitFramegraph();
}


void Renderer::InitFramegraph()
{
    m_framegraph.ConstructAndEnableNode<DepthPrepassNode>( m_depth_stencil_format_dsv, *m_device );

    m_framegraph.ConstructAndEnableNode<ShadowPassNode>( m_depth_stencil_format_dsv, m_shadow_bias, *m_device );
    m_framegraph.ConstructAndEnableNode<PSSMNode>( m_depth_stencil_format_dsv, m_shadow_bias, *m_device );

    m_framegraph.ConstructAndEnableNode<ForwardPassNode>( m_hdr_format, // rendertarget
                                                          m_hdr_format, // ambient lighting
                                                          m_normals_format, // normals
                                                          m_depth_stencil_format_dsv, *m_device );

    m_framegraph.ConstructAndEnableNode<SkyboxNode>( m_hdr_format, m_depth_stencil_format_dsv, *m_device );

    m_framegraph.ConstructAndEnableNode<HBAONode>( m_ssao->GetFormat(), *m_device );
    m_framegraph.ConstructAndEnableNode<BlurSSAONode>( *m_device );
    m_framegraph.ConstructAndEnableNode<ToneMapNode>( m_back_buffer_format, *m_device );
    m_framegraph.ConstructAndEnableNode<LightComposeNode>( m_back_buffer_format, *m_device );

    if ( m_framegraph.IsRebuildNeeded() )
        m_framegraph.Rebuild();
}


void Renderer::CreateTransientResources()
{
    // little hack here, create 1x1 textures and resize them right after

    auto create_tex = [&]( const wchar_t* name, std::unique_ptr<DynamicTexture>& tex, DXGI_FORMAT texture_format,
                           bool create_srv_table, bool create_uav_table, bool create_rtv_desc, bool create_dsv_desc )
    {
        DynamicTexture::ViewsToCreate views_to_create;
        views_to_create
        tex = std::make_unique<DynamicTexture>( name, 1, 1, texture_format, )

        CD3DX12_RESOURCE_DESC desc( CD3DX12_RESOURCE_DESC::Tex2D( texture_format,
                                                                  /*width=*/1, /*height=*/1,
                                                                  /*depth=*/1, /*mip_levels=*/1 ) );

        D3D12_CLEAR_VALUE clear_value;
        clear_value.Format = create_dsv_desc ? m_depth_stencil_format_dsv : texture_format;
        if ( create_dsv_desc )
        {
            clear_value.DepthStencil.Depth = 0.0f;
            clear_value.DepthStencil.Stencil = 0;
        }
        else
        {
            clear_value.Color[0] = 0;
            clear_value.Color[1] = 0;
            clear_value.Color[2] = 0;
            clear_value.Color[3] = 0;
        }

        if ( create_uav_table )
            desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        if ( create_rtv_desc )
            desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if ( create_dsv_desc )
            desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        // create resource (maybe create it in DynamicTexture?)
        {
            const D3D12_RESOURCE_STATES initial_state = create_dsv_desc ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_COMMON;

            const auto* opt_clear_ptr = (create_rtv_desc || create_dsv_desc) ? &clear_value : nullptr;
            ComPtr<ID3D12Resource> res;
            ThrowIfFailedH( m_device->CreateCommittedResource( &CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ), D3D12_HEAP_FLAG_NONE,
                                                              &desc, initial_state,
                                                              opt_clear_ptr,
                                                              IID_PPV_ARGS( res.GetAddressOf() ) ) );

            tex = std::make_unique<DynamicTexture>( std::move( res ), m_device, initial_state, opt_clear_ptr );
        }

        if ( create_srv_table )
            tex->SRV() = m_descriptor_tables->AllocateTable( 1 );
        if ( create_uav_table )
            tex->UAV() = m_descriptor_tables->AllocateTable( 1 );
        if ( create_rtv_desc )
            tex->RTV() = std::make_unique<Descriptor>( std::move( m_rtv_heap.AllocateDescriptor() ) );
        if ( create_dsv_desc )
            tex->DSV() = std::make_unique<Descriptor>( std::move( m_dsv_heap.AllocateDescriptor() ) );
    };


    create_tex( m_hdr_backbuffer, m_hdr_format, true, false, true, false );
    create_tex( m_hdr_ambient, m_hdr_format, true, false, true, false );
    create_tex( m_normals, m_normals_format, true, false, true, false );
    create_tex( m_ssao, m_ssao_format, true, false, true, false );
    create_tex( m_ssao_blurred, m_ssao_format, true, true, false, false );
    create_tex( m_ssao_blurred_transposed, m_ssao_format, true, true, false, false );
    create_tex( m_depth_stencil_buffer, m_depth_stencil_format_resource, true, false, false, true );

    ResizeTransientResources();
}


void Renderer::ResizeTransientResources()
{
    auto resize_tex = [&]( const wchar_t* name, auto& tex, uint32_t width, uint32_t height, bool make_srv, bool make_uav, bool make_rtv )
    {
        tex.Resize( width, height );
        if ( make_srv )
            m_device->CreateShaderResourceView( tex.Resource(), nullptr, *m_descriptor_tables->ModifyTable( tex.SRV() ) );
        if ( make_uav )
            m_device->CreateUnorderedAccessView( tex.Resource(), nullptr, nullptr, *m_descriptor_tables->ModifyTable( tex.UAV() ) );
        if ( make_rtv )
            m_device->CreateRenderTargetView( tex.Resource(), nullptr, tex.RTV()->HandleCPU() );

        tex.Resource()->SetName( name );
    };

    resize_tex( L"hdr buffer", *m_hdr_backbuffer, m_resolution_width, m_resolution_height, true, false, true );
    resize_tex( L"hdr ambient", *m_hdr_ambient, m_resolution_width, m_resolution_height, true, false, true );
    resize_tex( L"normal buffer", *m_normals, m_resolution_width, m_resolution_height, true, false, true );
    resize_tex( L"ssao", *m_ssao, m_resolution_width / 2, m_resolution_height / 2, true, false, true );
    resize_tex( L"blurred ssao", *m_ssao_blurred, m_resolution_width, m_resolution_height, true, true, false );
    resize_tex( L"transposed ssao", *m_ssao_blurred_transposed, m_resolution_height, m_resolution_width, true, true, false );

    resize_tex( L"depth stencil buffer", *m_depth_stencil_buffer, m_resolution_width, m_resolution_height, false, false, false );
    {
        auto& tex = *m_depth_stencil_buffer;
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc;
        dsv_desc.Format = m_depth_stencil_format_dsv;
        dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv_desc.Texture2D.MipSlice = 0;
        dsv_desc.Flags = D3D12_DSV_FLAG_NONE;

        m_device->CreateDepthStencilView( tex.Resource(), &dsv_desc, tex.DSV()->HandleCPU() );

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
        srv_desc.Format = m_depth_stencil_format_srv;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Texture2D.MostDetailedMip = 0;
        srv_desc.Texture2D.MipLevels = 1;
        srv_desc.Texture2D.PlaneSlice = 0;
        srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;
        m_device->CreateShaderResourceView( tex.Resource(), &srv_desc, *m_descriptor_tables->ModifyTable( tex.SRV() ) );
    }
}

D3D12_HEAP_DESC Renderer::GetUploadHeapDescription() const
{
    constexpr UINT64 heap_block_size = 1024 * 1024;
    return CD3DX12_HEAP_DESC( heap_block_size, D3D12_HEAP_TYPE_UPLOAD, 0, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS );
}


RenderTask Renderer::CreateTask( const Camera::Data& main_camera, const span<Light>& light_list, RenderMode mode, GPULinearAllocator& cb_allocator ) const
{
    OPTICK_EVENT();
    if ( mode != RenderMode::FullTonemapped )
        NOTIMPL;

    ForwardCBProvider forward_cb = ForwardCBProvider::Create( main_camera, m_pssm, light_list, *m_device, cb_allocator );

    RenderTask new_task( std::move( forward_cb ) );
    new_task.m_mode = mode;

    new_task.m_shadow_frustrums = ShadowProvider::Update( light_list, m_pssm, main_camera );

    new_task.m_main_frustrum = RenderTask::Frustrum{ RenderTask::Frustrum::Type::Perspective,
                                                     forward_cb.GetProj(),
                                                     forward_cb.GetView(),
                                                     DirectX::XMLoadFloat4x4( &forward_cb.GetViewProj() ) };

    return std::move( new_task );
}


void Renderer::Draw( const RenderTask& task, const SceneContext& scene_ctx, const FrameContext& frame_ctx,
                     std::vector<CommandList>& graphics_cmd_lists )
{
    OPTICK_EVENT();
    assert( frame_ctx.cmd_list_pool != nullptr );
    assert( frame_ctx.resources != nullptr );

    m_framegraph.ClearResources();

    if ( m_framegraph.IsRebuildNeeded() )
        m_framegraph.Rebuild();

    
    GPULinearAllocator upload_allocator( m_device, GetUploadHeapDescription() );

    frame_ctx.resources->AddResources( make_single_elem_span( task.m_forward_cb_provider.GetResource() ) );

    {
        ShadowProducers producers;
        ShadowMaps sm_storage;
        ShadowCascadeProducers pssm_producers;
        ShadowCascade pssm_storage;
        m_shadow_provider.FillFramegraphStructures( task.m_forward_cb_provider.GetLightsInCB(), make_span( task.m_shadow_renderlists[0][0] ),
                                                    producers, pssm_producers, sm_storage, pssm_storage );
        m_framegraph.SetRes( producers );
        m_framegraph.SetRes( sm_storage );
        m_framegraph.SetRes( pssm_producers );
        m_framegraph.SetRes( pssm_storage );
    }

    {
        MainRenderitems forward_renderitems;
        forward_renderitems.items = make_span( task.m_opaque_renderlist );
        m_framegraph.SetRes( forward_renderitems );
    }

    // Reuse memory associated with command recording
    // We can only reset when the associated command lists have finished execution on GPU
    CommandList cmd_list = frame_ctx.cmd_list_pool->GetList( D3D12_COMMAND_LIST_TYPE_DIRECT );
    cmd_list.Reset();

    ID3D12GraphicsCommandList* list_iface = cmd_list.GetInterface();

    constexpr uint32_t nbarriers = 9;
    CD3DX12_RESOURCE_BARRIER rtv_barriers[nbarriers];
    rtv_barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition( m_hdr_backbuffer->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET );
    rtv_barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition( m_hdr_ambient->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET );
    rtv_barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition( m_normals->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET );
    rtv_barriers[3] = CD3DX12_RESOURCE_BARRIER::Transition( m_ssao->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET );
    rtv_barriers[4] = CD3DX12_RESOURCE_BARRIER::Transition( m_ssao_blurred->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
    rtv_barriers[5] = CD3DX12_RESOURCE_BARRIER::Transition( m_ssao_blurred_transposed->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
    
    {
        auto& sm_storage = m_framegraph.GetRes<ShadowMaps>();
        if ( ! sm_storage )
            throw SnowEngineException( "missing resource" );
        rtv_barriers[6] = CD3DX12_RESOURCE_BARRIER::Transition( sm_storage->res, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE );
        auto& pssm_storage = m_framegraph.GetRes<ShadowCascade>();
        if ( ! pssm_storage )
            throw SnowEngineException( "missing resource" );
        rtv_barriers[7] = CD3DX12_RESOURCE_BARRIER::Transition( pssm_storage->res, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE );
    }
    rtv_barriers[8] = CD3DX12_RESOURCE_BARRIER::Transition( m_depth_stencil_buffer->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE );

    list_iface->ResourceBarrier( nbarriers, rtv_barriers );
    ID3D12DescriptorHeap* heaps[] = { m_descriptor_tables->CurrentGPUHeap().Get() };
    list_iface->SetDescriptorHeaps( 1, heaps );

    {
        auto skybox_framegraph_res = CreateSkybox( scene_ctx.skybox, scene_ctx.ibl_table, upload_allocator );
        frame_ctx.resources->AddResources( make_single_elem_span( skybox_framegraph_res.second ) );

        m_framegraph.SetRes( skybox_framegraph_res.first );

        ForwardPassCB pass_cb{ task.m_forward_cb_provider.GetResource()->GetGPUVirtualAddress() };
        m_framegraph.SetRes( pass_cb );

        HDRBuffer hdr_buffer;
        {
            hdr_buffer.res = m_hdr_backbuffer->GetResource();
            hdr_buffer.nmips = m_hdr_backbuffer->GetMipCount();
            hdr_buffer.rtv.resize( hdr_buffer.nmips );
            hdr_buffer.srv.resize( hdr_buffer.nmips );
            hdr_buffer.rtv[0] = m_hdr_backbuffer->GetRtvPerMip()[0].HandleCPU();
            hdr_buffer.rtv[1] = m_hdr_backbuffer->GetRtvPerMip()[1].HandleCPU();
            auto srv_per_mip = m_hdr_backbuffer->GetSrvPerMip();
            hdr_buffer.srv[0] = GetGPUHandle( *srv_per_mip, 0 );
            hdr_buffer.srv[1] = GetGPUHandle( *srv_per_mip, 1 );
        }
        m_framegraph.SetRes( hdr_buffer );

        AmbientBuffer ambient_buffer;
        ambient_buffer.res = m_hdr_ambient->GetResource();
        ambient_buffer.rtv = m_hdr_ambient->GetRtvPerMip()[0].HandleCPU();
        ambient_buffer.srv = GetGPUHandle( *m_hdr_ambient->GetSrvMain(), 0 );
        m_framegraph.SetRes( ambient_buffer );

        NormalBuffer normal_buffer;
        normal_buffer.res = m_normals->GetResource();
        normal_buffer.rtv = m_normals->GetRtvPerMip()[0].HandleCPU();
        normal_buffer.srv = GetGPUHandle( *m_normals->GetSrvMain(), 0 );
        m_framegraph.SetRes( normal_buffer );

        DepthStencilBuffer depth_buffer;
        depth_buffer.dsv = m_depth_stencil_buffer->GetDsvPerMip()[0].HandleCPU();
        depth_buffer.srv = GetGPUHandle( *m_depth_stencil_buffer->GetSrvMain(), 0 );
        depth_buffer.res = m_depth_stencil_buffer->GetResource();
        m_framegraph.SetRes( depth_buffer );

        ScreenConstants screen;
        screen.viewport = CD3DX12_VIEWPORT( m_hdr_backbuffer->GetResource() );
        screen.scissor_rect = CD3DX12_RECT( 0, 0, screen.viewport.Width, screen.viewport.Height );
        m_framegraph.SetRes( screen );

        SSAOBuffer_Noisy ssao_texture;
        ssao_texture.res = m_ssao->GetResource();
        ssao_texture.rtv = m_ssao->GetRtvPerMip()[0].HandleCPU();
        ssao_texture.srv = GetGPUHandle( *m_ssao->GetSrvMain(), 0 );
        m_framegraph.SetRes( ssao_texture );

        SSAOTexture_Blurred ssao_blurred_texture;
        ssao_blurred_texture.res = m_ssao_blurred->GetResource();
        ssao_blurred_texture.uav = GetGPUHandle( *m_ssao_blurred->GetUavPerMip(), 0 );
        ssao_blurred_texture.srv = GetGPUHandle( *m_ssao_blurred->GetSrvMain(), 0 );
        m_framegraph.SetRes( ssao_blurred_texture );

        SSAOTexture_Transposed ssao_blurred_texture_horizontal;
        ssao_blurred_texture_horizontal.res = m_ssao_blurred_transposed->GetResource();
        ssao_blurred_texture_horizontal.uav = GetGPUHandle( *m_ssao_blurred_transposed->GetUavPerMip(), 0 );
        ssao_blurred_texture_horizontal.srv = GetGPUHandle( *m_ssao_blurred_transposed->GetSrvMain(), 0 );
        m_framegraph.SetRes( ssao_blurred_texture_horizontal );


        TonemapNodeSettings tm_settings;
        tm_settings.data.blend_luminance = false;
        tm_settings.data.lower_luminance_bound = m_tonemap_settings.min_luminance;
        tm_settings.data.upper_luminance_bound = m_tonemap_settings.max_luminance;
        m_framegraph.SetRes( tm_settings );

        ::HBAOSettings hbao_settings;
        hbao_settings.data.render_target_size = DirectX::XMFLOAT2( screen.viewport.Width, screen.viewport.Height );
        hbao_settings.data.angle_bias = m_hbao_settings.angle_bias;
        hbao_settings.data.max_r = m_hbao_settings.max_r;
        hbao_settings.data.nsamples_per_direction = m_hbao_settings.nsamples_per_direction;
        m_framegraph.SetRes( hbao_settings );

        SDRBuffer backbuffer;
        {
            const auto& target = frame_ctx.render_target;
            backbuffer.res = target.resource;
            backbuffer.rtv = target.rtv;
            backbuffer.viewport = target.viewport;
            backbuffer.scissor_rect = target.scissor_rect;
        }
        m_framegraph.SetRes( backbuffer );
    }

    m_framegraph.Run( *list_iface );

    ThrowIfFailedH( list_iface->Close() );

    graphics_cmd_lists.emplace_back( std::move( cmd_list ) );

    frame_ctx.resources->AddAllocators( make_single_elem_span( upload_allocator ) );
}


void Renderer::SetSkybox( bool enable )
{
    if ( enable )
        m_framegraph.Enable<SkyboxNode>();
    else
        m_framegraph.Disable<SkyboxNode>();
}


void Renderer::SetInternalResolution( uint32_t width, uint32_t height )
{
    m_resolution_width = width;
    m_resolution_height = height;
    ResizeTransientResources();
}


DXGI_FORMAT Renderer::GetTargetFormat( RenderMode mode ) const noexcept
{
    return m_back_buffer_format;
}


void Renderer::SetTargetFormat( RenderMode mode, DXGI_FORMAT format )
{
    if ( mode != RenderMode::FullTonemapped )
        NOTIMPL;

    m_back_buffer_format = format;

    if ( m_back_buffer_format != DefaultBackbufferFormat )
        throw SnowEngineException( "unsupported rtv format" );

    m_framegraph.ConstructNode<ToneMapNode>( m_back_buffer_format, *m_device );
}


void Renderer::MakeObjectCB( const DirectX::XMMATRIX& obj2world, GPUObjectConstants& object_cb )
{
    XMStoreFloat4x4( &object_cb.model, XMMatrixTranspose( obj2world ) );
    XMStoreFloat4x4( &object_cb.model_inv_transpose, XMMatrixTranspose( InverseTranspose( obj2world ) ) );
}


Renderer::RenderBatchList Renderer::CreateRenderitems( const span<const RenderItem>& render_list, GPULinearAllocator& frame_allocator ) const
{
    OPTICK_EVENT();
    RenderBatchList items;

    if ( render_list.size() == 0 )
        return items;

    items.batches.reserve( render_list.size() );

    constexpr UINT obj_cb_stride = Utils::CalcConstantBufferByteSize( sizeof( GPUObjectConstants ) ) ;
    const uint64_t buffer_size = obj_cb_stride * render_list.size();

    auto gpu_allocation = frame_allocator.Alloc( buffer_size );

    ThrowIfFailedH( m_device->CreatePlacedResource( gpu_allocation.first, gpu_allocation.second, 
            &CD3DX12_RESOURCE_DESC::Buffer( buffer_size ),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS( items.per_obj_cb.GetAddressOf() ) ) );

    char* mapped_cb = nullptr;
    ThrowIfFailedH( items.per_obj_cb->Map( 0, nullptr, (void**)&mapped_cb ) );

    D3D12_GPU_VIRTUAL_ADDRESS gpu_addr = items.per_obj_cb->GetGPUVirtualAddress();

    for ( const auto& item : render_list )
    {
        GPUObjectConstants obj_cb;
        MakeObjectCB( DirectX::XMLoadFloat4x4( &item.local2world ), obj_cb );
        memcpy( mapped_cb, &obj_cb, sizeof( obj_cb ) );

        items.batches.emplace_back();
        auto& batch = items.batches.back();
        
        batch.per_object_cb = gpu_addr;
        gpu_addr += obj_cb_stride;
        mapped_cb += obj_cb_stride;

        batch.item_id = item.item_id;
        batch.vbv = item.vbv;
        batch.ibv = item.ibv;
        batch.material = item.material;
        batch.instance_count = 1;
        batch.index_count = item.index_count;
        batch.index_offset = item.index_offset;
        batch.vertex_offset = item.vertex_offset;
    }

    return std::move( items );
}


std::pair<Skybox, ComPtr<ID3D12Resource>> Renderer::CreateSkybox( const SkyboxData& skybox, DescriptorTableID ibl_table, GPULinearAllocator& upload_cb_allocator ) const
{
    std::pair<Skybox, ComPtr<ID3D12Resource>> res;
    res.first.srv_skybox = skybox.cubemap_srv;

    {
        D3D12_GPU_DESCRIPTOR_HANDLE desc_table = m_descriptor_tables->GetTable( ibl_table )->gpu_handle;
        res.first.srv_table = desc_table;
    }

    res.second = CreateSkyboxCB( skybox.obj2world_mat, upload_cb_allocator );
    res.first.tf_cbv = res.second->GetGPUVirtualAddress();

    res.first.radiance_factor = skybox.radiance_factor;

    return std::move( res );
}


ComPtr<ID3D12Resource> Renderer::CreateSkyboxCB( const DirectX::XMFLOAT4X4& obj2world, GPULinearAllocator& upload_cb_allocator ) const
{
    ComPtr<ID3D12Resource> res;

    constexpr uint64_t cb_size = sizeof( obj2world );
    auto allocation = upload_cb_allocator.Alloc( cb_size );
    ThrowIfFailedH( m_device->CreatePlacedResource( allocation.first, UINT64( allocation.second ),
                                                    &CD3DX12_RESOURCE_DESC::Buffer( cb_size ), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                    IID_PPV_ARGS( res.GetAddressOf() ) ) );

    DirectX::XMFLOAT4X4* mapped_cb; 
    ThrowIfFailedH( res->Map( 0, nullptr, (void**)&mapped_cb ) );

    DirectX::XMMATRIX transposed_mat = DirectX::XMMatrixTranspose( DirectX::XMLoadFloat4x4( &obj2world ) );
    DirectX::XMStoreFloat4x4( mapped_cb, transposed_mat );
    res->Unmap( 0, nullptr );

    return std::move( res );
}

D3D12_GPU_DESCRIPTOR_HANDLE Renderer::GetGPUHandle( DynamicTexture::TextureView view, int mip ) const
{
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(
        m_descriptor_tables->GetTable( view.table )->gpu_handle,
        view.table_offset + mip,
        m_descriptor_tables->GetDescriptorIncrementSize()
    );
}
