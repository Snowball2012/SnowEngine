// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "SceneManager.h"
#include "StaticTextureManager.h"

#include <dxtk12/DDSTextureLoader.h>
#include <dxtk12/DirectXHelpers.h>

StaticTextureManager::StaticTextureManager( Microsoft::WRL::ComPtr<ID3D12Device> device, Scene* scene ) noexcept
    : m_device( std::move( device ) ), m_scene( scene ), m_srv_heap( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_device.Get() )
{
}

StaticTextureManager::~StaticTextureManager()
{
    for ( auto& tex_data : boost::range::join( m_loaded_textures, m_pending_transaction.textures_to_upload ) )
        tex_data.staging_srv.reset();
    for ( auto& transaction : m_active_transactions )
        for ( auto& tex_data : transaction.transaction.textures_to_upload )
            tex_data.staging_srv.reset();
}


void StaticTextureManager::LoadTexture( TextureID id, std::string path )
{
    m_pending_transaction.textures_to_upload.emplace_back();
    auto& tex_data = m_pending_transaction.textures_to_upload.back();

    tex_data.id = id;

    std::unique_ptr<uint8_t[]> dds_data;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    ThrowIfFailed( DirectX::LoadDDSTextureFromFileEx( m_device.Get(),
                                                      std::wstring( path.cbegin(), path.cend() ).c_str(), 0,
                                                      D3D12_RESOURCE_FLAG_NONE,
                                                      DirectX::DDS_LOADER_DEFAULT | DirectX::DDS_LOADER_CREATE_IN_COMMON_STATE,
                                                      tex_data.gpu_res.GetAddressOf(), dds_data, subresources ) );

    tex_data.path = std::move( path );

    m_pending_transaction.uploaders.push_back(
        CreateAndFillUploader( tex_data.gpu_res, make_span( subresources.data(), subresources.data() + subresources.size() ) ) );

    tex_data.staging_srv = std::make_unique<Descriptor>( std::move( m_srv_heap.AllocateDescriptor() ) );

    DirectX::CreateShaderResourceView( m_device.Get(), tex_data.gpu_res.Get(), tex_data.staging_srv->HandleCPU() );
}


void StaticTextureManager::Update( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp current_timestamp, ID3D12GraphicsCommandList& cmd_list )
{
    FinalizeCompletedTransactions( current_timestamp );
    EnqueueRemovedTexturesForDestruction();

    if ( m_pending_transaction.textures_to_upload.empty() && m_pending_transaction.textures_to_remove.empty() )
        return; // nothing to do

    if ( m_pending_transaction.textures_to_upload.size() != m_pending_transaction.uploaders.size() )
        throw SnowEngineException( "number of textures must match the number of uploaders" );

    for ( size_t tex_idx = 0; tex_idx < m_pending_transaction.textures_to_upload.size(); tex_idx++ )
    {
        UploadTexture( m_pending_transaction.uploaders[tex_idx],
                       m_pending_transaction.textures_to_upload[tex_idx].gpu_res.Get(),
                       cmd_list );
    }

    m_active_transactions.push_back( ActiveTransaction{ std::move( m_pending_transaction ), operation_tag, std::nullopt } );

    m_pending_transaction.textures_to_remove.clear();
    m_pending_transaction.textures_to_upload.clear();
    m_pending_transaction.uploaders.clear();
}


void StaticTextureManager::PostTimestamp( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp end_timestamp )
{
    for ( auto& transaction : m_active_transactions )
        if ( transaction.operation_tag == operation_tag )
        {
            if ( transaction.end_timestamp.has_value() )
                throw SnowEngineException( "operation already has a timestamp" );
            else
                transaction.end_timestamp = end_timestamp;
        }
}


void StaticTextureManager::LoadEverythingBeforeTimestamp( GPUTaskQueue::Timestamp timestamp )
{
    for ( auto& active_transaction : m_active_transactions )
    {
        if ( active_transaction.end_timestamp.has_value() && active_transaction.end_timestamp.value() <= timestamp )
            LoadTexturesFromTransaction( active_transaction.transaction );
    }
}


void StaticTextureManager::LoadTexturesFromTransaction( UploadTransaction& transaction )
{
    m_loaded_textures.reserve( m_loaded_textures.size() + transaction.textures_to_upload.size() );

    for ( auto& tex_data : transaction.textures_to_upload )
    {
        Texture* tex = m_scene->TryModifyTexture( tex_data.id );
        if ( tex ) // mesh could have already been deleted
        {
            tex->Load( tex_data.staging_srv->HandleCPU() );
            m_loaded_textures.push_back( std::move( tex_data ) );
        }
    }
    transaction.textures_to_upload.clear();
}


void StaticTextureManager::FinalizeCompletedTransactions( GPUTaskQueue::Timestamp current_timestamp )
{
    for ( size_t i = 0; i < m_active_transactions.size(); )
    {
        auto& active_transaction = m_active_transactions[i];

        if ( active_transaction.end_timestamp.has_value()
             && active_transaction.end_timestamp.value() <= current_timestamp )
        {
            LoadTexturesFromTransaction( active_transaction.transaction );
            if ( i != m_active_transactions.size() - 1 )
                active_transaction = std::move( m_active_transactions.back() );

            m_active_transactions.pop_back();
        }
        else
        {
            i++;
        }
    }
}


void StaticTextureManager::EnqueueRemovedTexturesForDestruction()
{
    for ( size_t i = 0; i < m_loaded_textures.size(); )
    {
        auto& tex_data = m_loaded_textures[i];

        Texture* tex = m_scene->TryModifyTexture( tex_data.id );
        if ( ! tex )
        {
            // remove texture
            m_pending_transaction.textures_to_remove.push_back( std::move( tex_data ) );
            if ( i != m_loaded_textures.size() - 1 )
                tex_data = std::move( m_loaded_textures.back() );

            m_loaded_textures.pop_back();
        }
        else
        {
            i++;
        }
    }
}


void StaticTextureManager::UploadTexture( const UploadData& uploader, ID3D12Resource* tex_gpu, ID3D12GraphicsCommandList& cmd_list )
{
    for ( size_t subres_idx = 0; subres_idx < uploader.footprints.size(); ++subres_idx )
    {
        CD3DX12_TEXTURE_COPY_LOCATION dst( tex_gpu, UINT( subres_idx ) );
        CD3DX12_TEXTURE_COPY_LOCATION src( uploader.resource.Get(), uploader.footprints[subres_idx] );
        cmd_list.CopyTextureRegion( &dst, 0, 0, 0, &src, nullptr );
    }
}


StaticTextureManager::UploadData StaticTextureManager::CreateAndFillUploader( const Microsoft::WRL::ComPtr<ID3D12Resource>& gpu_res, const span<D3D12_SUBRESOURCE_DATA>& subresources )
{
    UploadData uploader;

    D3D12_RESOURCE_DESC res_desc = gpu_res->GetDesc();

    const size_t nsubres = subresources.size();

    uploader.footprints.resize( nsubres );
    auto& footprints = uploader.footprints;

    std::vector<UINT> nrows( nsubres );
    std::vector<UINT64> row_size( nsubres );
    UINT64 required_size = 0;
    m_device->GetCopyableFootprints( &res_desc, 0, UINT( nsubres ), 0, footprints.data(), nrows.data(), row_size.data(), &required_size );

    ThrowIfFailed( m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD ),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer( required_size ),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS( uploader.resource.GetAddressOf() ) ) );

    uint8_t* mapped_uploader;
    ThrowIfFailed( uploader.resource->Map( 0, nullptr, reinterpret_cast<void**>( &mapped_uploader ) ) );

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

    uploader.resource->Unmap( 0, nullptr );

    return std::move( uploader );
}
