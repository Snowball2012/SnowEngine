// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "CubemapManager.h"
#include "SceneManager.h"
#include "CubemapConverter.h"

#include <dxtk12/DDSTextureLoader.h>
#include <dxtk12/DirectXHelpers.h>

CubemapManager::CubemapManager( Microsoft::WRL::ComPtr<ID3D12Device> device, DescriptorTableBakery* descriptor_tables, Scene* scene ) noexcept
    : m_device( std::move( device ) ), m_scene( scene )
    , m_srv_heap( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_device.Get() )
    , m_desc_tables( descriptor_tables )
    , m_converter( m_device )
{
}

CubemapManager::~CubemapManager()
{
}


void CubemapManager::CreateCubemapFromTexture( CubemapID cubemap_id, TextureID texture_id )
{
    assert( m_scene->AllTextures().has( texture_id ) );
    assert( m_scene->AllCubemaps().has( cubemap_id ) );

    m_conversion_in_progress.emplace_back();
    auto& new_cubemap = m_conversion_in_progress.back();
    new_cubemap.texture_id = texture_id;

    m_scene->TryModifyTexture( texture_id )->AddRef();

    new_cubemap.texture_srv = DescriptorTableBakery::TableID::nullid;
    new_cubemap.cubemap.id = cubemap_id;
}


void CubemapManager::LoadCubemap( CubemapID cubemap_id, const std::string& path )
{
    m_copy_in_progress.emplace_back();
    auto& new_copy = m_copy_in_progress.back();

    auto& cubemap_data = new_copy.cubemap;
    cubemap_data.id = cubemap_id;

    std::unique_ptr<uint8_t[]> dds_data;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;

    bool is_cubemap;

    ThrowIfFailedH( DirectX::LoadDDSTextureFromFileEx( m_device.Get(),
                                                      std::wstring( path.cbegin(), path.cend() ).c_str(), 0,
                                                      D3D12_RESOURCE_FLAG_NONE,
                                                      DirectX::DDS_LOADER_DEFAULT | DirectX::DDS_LOADER_CREATE_IN_COMMON_STATE,
                                                      cubemap_data.gpu_res.GetAddressOf(), dds_data, subresources, nullptr, &is_cubemap ) );

    if ( ! is_cubemap )
        throw SnowEngineException( "not cubemap" );

    cubemap_data.staging_srv = std::make_unique<Descriptor>( std::move( m_srv_heap.AllocateDescriptor() ) );
    DirectX::CreateShaderResourceView( m_device.Get(), cubemap_data.gpu_res.Get(), cubemap_data.staging_srv->HandleCPU(), true );

    FillUploader( new_copy, make_span( subresources ) );
}


void CubemapManager::Update( GPUTaskQueue::Timestamp current_timestamp, SceneCopyOp copy_op, ID3D12GraphicsCommandList& cmd_list )
{
    // Todo: remove missing cubemaps

    for ( auto& copy : m_copy_in_progress )
    {
        if ( ! ( copy.end_timestamp && *copy.end_timestamp <= current_timestamp ) )
            continue;

        m_loaded_cubemaps.emplace_back( std::move( copy.cubemap ) );

        Cubemap* cubemap = m_scene->TryModifyCubemap( m_loaded_cubemaps.back().id );

        cubemap->Load( m_loaded_cubemaps.back().staging_srv->HandleCPU() );
    }

    m_copy_in_progress.erase( std::remove_if( m_copy_in_progress.begin(), m_copy_in_progress.end(),
                                              [&]( const auto& copy ) { return copy.end_timestamp && *copy.end_timestamp <= current_timestamp; } ),
                              m_copy_in_progress.end() );

    for ( auto& copy : m_copy_in_progress )
    {
        if ( copy.operation_tag )
            continue;

        copy.operation_tag = copy_op;

        for ( size_t subres_idx = 0; subres_idx < copy.footprints.size(); ++subres_idx )
        {
            CD3DX12_TEXTURE_COPY_LOCATION dst( copy.cubemap.gpu_res.Get(), UINT( subres_idx ) );
            CD3DX12_TEXTURE_COPY_LOCATION src( copy.upload_res.Get(), copy.footprints[subres_idx] );
            cmd_list.CopyTextureRegion( &dst, 0, 0, 0, &src, nullptr );
        }
    }

    for ( auto i = m_conversion_in_progress.begin(); i != m_conversion_in_progress.end(); )
    {
        auto& cubemap = *i;

        const auto& texture = m_scene->AllTextures()[cubemap.texture_id];
        if ( texture.IsLoaded() )
        {
            cubemap.texture_srv = m_desc_tables->AllocateTable( 1 );
            m_device->CopyDescriptorsSimple( 1, m_desc_tables->ModifyTable( cubemap.texture_srv ).value(),
                                             texture.StagingSRV(),
                                             D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
        }
        if ( cubemap.cubemap.staging_srv )
        {
            m_scene->TryModifyCubemap( i->cubemap.id )->Load( i->cubemap.staging_srv->HandleCPU() );
            m_loaded_cubemaps.emplace_back( std::move( i->cubemap ) );
            i = m_conversion_in_progress.erase( i ); // TODO: change to swap-erase, check for cubemap existance
        }
        else
        {
            ++i;
        }
    }
}


void CubemapManager::PostTimestamp( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp end_timestamp )
{
    for ( auto& copy : m_copy_in_progress )
    {
        if ( ! copy.operation_tag )
            continue;

        if ( *copy.operation_tag != operation_tag )
            continue;

        copy.end_timestamp = end_timestamp;
    }
}


void CubemapManager::OnBakeDescriptors( ID3D12GraphicsCommandList& cmd_list_graphics_queue )
{
    for ( auto i = m_conversion_in_progress.begin(); i != m_conversion_in_progress.end(); ++i )
    {
        if ( ! ( i->texture_srv == DescriptorTableBakery::TableID::nullid ) )
        {
            i->cubemap.gpu_res = m_converter.MakeCubemapFromCylindrical( CubemapResolution,
                                                                              m_desc_tables->GetTable( i->texture_srv )->gpu_handle,
                                                                              DXGI_FORMAT_R16G16B16A16_FLOAT, cmd_list_graphics_queue );

            if ( ! i->cubemap.gpu_res )
                throw SnowEngineException( "convertation failed" );

            cmd_list_graphics_queue.ResourceBarrier( 1,
                                                     &CD3DX12_RESOURCE_BARRIER::Transition( i->cubemap.gpu_res.Get(),
                                                                                            D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ) );

            i->cubemap.staging_srv = std::make_unique<Descriptor>( std::move( m_srv_heap.AllocateDescriptor() ) );
            DirectX::CreateShaderResourceView( m_device.Get(), i->cubemap.gpu_res.Get(), i->cubemap.staging_srv->HandleCPU(), true );
        }
    }
}


void CubemapManager::FillUploader( CopyData& data, const span<D3D12_SUBRESOURCE_DATA>& subresources )
{
    D3D12_RESOURCE_DESC res_desc = data.cubemap.gpu_res->GetDesc();

    const size_t nsubres = subresources.size();

    auto& footprints = data.footprints;
    footprints.resize( nsubres );

    std::vector<UINT> nrows( nsubres );
    std::vector<UINT64> row_size( nsubres );
    UINT64 required_size = 0;
    m_device->GetCopyableFootprints( &res_desc, 0, UINT( nsubres ), 0, footprints.data(), nrows.data(), row_size.data(), &required_size );

    ThrowIfFailedH( m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD ),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer( required_size ),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS( data.upload_res.GetAddressOf() ) ) );

    uint8_t* mapped_uploader;
    ThrowIfFailedH( data.upload_res->Map( 0, nullptr, reinterpret_cast<void**>( &mapped_uploader ) ) );

    for ( size_t i = 0; i < nsubres; ++i )
    {
        const auto& subres_footprint = footprints[i].Footprint;
        D3D12_MEMCPY_DEST dest_data =
        {
            mapped_uploader + footprints[i].Offset,
            subres_footprint.RowPitch,
            subres_footprint.RowPitch * nrows[i]
        };

        MemcpySubresource( &dest_data, &subresources[i], SIZE_T( row_size[i] ), nrows[i], subres_footprint.Depth );
    }

    data.upload_res->Unmap( 0, nullptr );
}
