#include "stdafx.h"

#include "SceneManager.h"
#include "TextureStreamer.h"

#include <dxtk12/DDSTextureLoader.h>
#include <dxtk12/DirectXHelpers.h>


TextureStreamer::TextureStreamer( Microsoft::WRL::ComPtr<ID3D12Device> device, Scene* scene ) noexcept
	: m_device( std::move( device ) ), m_scene( scene ), m_srv_heap( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_device )
{
}

TextureStreamer::~TextureStreamer()
{
	for ( auto& tex_data : boost::range::join( m_loaded_textures, m_pending_transaction.textures_to_upload ) )
		tex_data.staging_srv.reset();
	for ( auto& transaction : m_active_transactions )
		for ( auto& tex_data : transaction.transaction.textures_to_upload )
			tex_data.staging_srv.reset();
}


void TextureStreamer::LoadStreamedTexture( TextureID id, std::string path )
{
	m_pending_transaction.textures_to_upload.emplace_back();
	auto& tex_data = m_pending_transaction.textures_to_upload.back();

	tex_data.id = id;

	if ( ! InitMemoryMappedTexture( path, tex_data ) )
		throw SnowEngineException( "failed to load the texture" );

	D3D12_RESOURCE_DESC commited_desc = tex_data.gpu_res->GetDesc();
	commited_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	tex_data.gpu_res.Reset();
	ThrowIfFailed( m_device->CreateCommittedResource( &CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ),
													  D3D12_HEAP_FLAG_NONE,
													  &commited_desc,
													  D3D12_RESOURCE_STATE_COMMON, nullptr,
													  IID_PPV_ARGS( tex_data.gpu_res.GetAddressOf() ) ) );

	tex_data.path = std::move( path );

	m_pending_transaction.uploaders.push_back(
		CreateAndFillUploader( tex_data.gpu_res, make_span( tex_data.subresources.data(), tex_data.subresources.data() + tex_data.subresources.size() ) ) );

	tex_data.staging_srv = std::make_unique<Descriptor>( std::move( m_srv_heap.AllocateDescriptor() ) );

	DirectX::CreateShaderResourceView( m_device.Get(), tex_data.gpu_res.Get(), tex_data.staging_srv->HandleCPU() );
}


void TextureStreamer::Update( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp current_timestamp, ID3D12GraphicsCommandList& cmd_list )
{
	FinalizeCompletedTransactions( current_timestamp );
	EnqueueRemovedTexturesForDestruction();

	if ( m_pending_transaction.textures_to_upload.empty() && m_pending_transaction.textures_to_remove.empty() )
		return; // nothing to do

	if ( m_pending_transaction.textures_to_upload.size() != m_pending_transaction.textures_to_upload.size() )
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


void TextureStreamer::PostTimestamp( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp end_timestamp )
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


void TextureStreamer::LoadEverythingBeforeTimestamp( GPUTaskQueue::Timestamp timestamp )
{
	for ( auto& active_transaction : m_active_transactions )
	{
		if ( active_transaction.end_timestamp.has_value() && active_transaction.end_timestamp.value() <= timestamp )
			LoadTexturesFromTransaction( active_transaction.transaction );
	}
}


void TextureStreamer::LoadTexturesFromTransaction( UploadTransaction& transaction )
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


void TextureStreamer::FinalizeCompletedTransactions( GPUTaskQueue::Timestamp current_timestamp )
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


void TextureStreamer::EnqueueRemovedTexturesForDestruction()
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


void TextureStreamer::UploadTexture( const UploadData& uploader, ID3D12Resource* tex_gpu, ID3D12GraphicsCommandList& cmd_list )
{
	for ( size_t subres_idx = 0; subres_idx < uploader.footprints.size(); ++subres_idx )
	{
		CD3DX12_TEXTURE_COPY_LOCATION dst( tex_gpu, UINT( subres_idx ) );
		CD3DX12_TEXTURE_COPY_LOCATION src( uploader.resource.Get(), uploader.footprints[subres_idx] );
		cmd_list.CopyTextureRegion( &dst, 0, 0, 0, &src, nullptr );
	}
}


TextureStreamer::UploadData TextureStreamer::CreateAndFillUploader( const Microsoft::WRL::ComPtr<ID3D12Resource>& gpu_res, const span<D3D12_SUBRESOURCE_DATA>& subresources )
{
	UploadData uploader;

	D3D12_RESOURCE_DESC res_desc = gpu_res->GetDesc();

	uploader.footprints.resize( subresources.size() );
	auto& footprints = uploader.footprints;

	std::vector<UINT> nrows( subresources.size() );
	std::vector<UINT64> row_size( subresources.size() );
	UINT64 required_size = 0;
	m_device->GetCopyableFootprints( &res_desc, 0, UINT( subresources.size() ), 0, footprints.data(), nrows.data(), row_size.data(), &required_size );

	ThrowIfFailed( m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD ),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer( required_size ),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS( uploader.resource.GetAddressOf() ) ) );

	uint8_t* mapped_uploader;
	ThrowIfFailed( uploader.resource->Map( 0, nullptr, reinterpret_cast<void**>( &mapped_uploader ) ) );	

	for ( size_t i = 0; i < subresources.size(); ++i )
	{
		D3D12_MEMCPY_DEST dest_data =
		{
			mapped_uploader + footprints[i].Offset,
			footprints[i].Footprint.RowPitch,
			footprints[i].Footprint.RowPitch * nrows[i]
		};

		MemcpySubresource( &dest_data, &subresources[i], SIZE_T( row_size[i] ), nrows[i], footprints[i].Footprint.Depth );
	}

	uploader.resource->Unmap( 0, nullptr );

	return std::move( uploader );
}


bool TextureStreamer::InitMemoryMappedTexture( const std::string_view& path, TextureData& data )
{
	// be careful with return value for failed winapi functions!
	// different functions return different values for handles!

	data.file_handle = CreateFileA( path.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
	if ( data.file_handle == INVALID_HANDLE_VALUE )
		return false;
	
	LARGE_INTEGER filesize;
	if ( ! GetFileSizeEx( data.file_handle, &filesize ) )
		return false;
	if ( filesize.QuadPart == 0 )
		return false;
	data.file_mapping = CreateFileMappingA( data.file_handle, nullptr, PAGE_READONLY, 0, 0, NULL );
	if ( ! data.file_mapping )
		return false;

	const uint8_t* mapped_region_start = reinterpret_cast<const uint8_t*>( MapViewOfFile( data.file_mapping, FILE_MAP_READ, 0, 0, 0 ) );
	if ( ! mapped_region_start )
		return false;

	data.mapped_file_data = span<const uint8_t>( mapped_region_start, mapped_region_start + filesize.QuadPart );

	HRESULT hr = DirectX::LoadDDSTextureFromMemoryEx( m_device.Get(),
													  data.mapped_file_data.cbegin(), filesize.QuadPart, 0,
													  D3D12_RESOURCE_FLAG_NONE,
													  DirectX::DDS_LOADER_DEFAULT | DirectX::DDS_LOADER_CREATE_RESERVED_RESOURCE,
													  data.gpu_res.GetAddressOf(), data.subresources );

	return SUCCEEDED( hr );
}
