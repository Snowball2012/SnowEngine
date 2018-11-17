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
	for ( auto& tex_data : m_loaded_textures )
		tex_data.mip_cumulative_srv.clear();
}


void TextureStreamer::LoadStreamedTexture( TextureID id, std::string path )
{
	m_textures_to_load.emplace_back();
	auto& tex_data = m_textures_to_load.back();

	tex_data.id = id;

	if ( ! tex_data.file.Open( path ) )
		throw SnowEngineException( "failed to open texture file" );

	tex_data.path = std::move( path );

	tex_data.gpu_res.Reset();
	ThrowIfFailed( DirectX::LoadDDSTextureFromMemoryEx( m_device.Get(),
														tex_data.file.GetData().cbegin(), tex_data.file.GetData().size(), 0,
														D3D12_RESOURCE_FLAG_NONE,
														DirectX::DDS_LOADER_DEFAULT | DirectX::DDS_LOADER_CREATE_RESERVED_RESOURCE,
														tex_data.gpu_res.GetAddressOf(), tex_data.file_layout ) );

	size_t subresource_num = tex_data.file_layout.size();
	const auto& res_desc = tex_data.gpu_res->GetDesc();
	UINT64 virtual_bytes_total = 0;
	tex_data.virtual_layout.nrows.resize( subresource_num );
	tex_data.virtual_layout.row_size.resize( subresource_num );
	m_device->GetCopyableFootprints( &res_desc, 0, tex_data.file_layout.size(), 0,
									 tex_data.virtual_layout.footprints.data(),
									 tex_data.virtual_layout.nrows.data(),
									 tex_data.virtual_layout.row_size.data(), &virtual_bytes_total );

	// Descriptors. Only 2d singular textures are currently supported.
	if ( tex_data.gpu_res->GetDesc().DepthOrArraySize > 1 )
		NOTIMPL;

	tex_data.mip_cumulative_srv.reserve( subresource_num );
	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.Format = tex_data.gpu_res->GetDesc().Format;
	srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = -1;
	srv_desc.Texture2D.PlaneSlice = 0;
	for ( size_t mip_idx = 0; mip_idx < subresource_num; ++mip_idx )
	{
		tex_data.mip_cumulative_srv.emplace_back( std::move( m_srv_heap.AllocateDescriptor() ) );

		srv_desc.Texture2D.MostDetailedMip = mip_idx;
		srv_desc.Texture2D.ResourceMinLODClamp = FLOAT( mip_idx );
		m_device->CreateShaderResourceView( tex_data.gpu_res.Get(), &srv_desc, tex_data.mip_cumulative_srv.back().HandleCPU() );
	}
}


void TextureStreamer::Update( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp current_timestamp, ID3D12GraphicsCommandList& cmd_list )
{
	NOTIMPL;

	for ( const auto& tex_data : m_loaded_textures )
	{
		auto& texture = *m_scene->TryModifyTexture( tex_data.id );
		if ( texture.IsLoaded() )
			texture.ModifyStagingSRV() = tex_data.mip_cumulative_srv[tex_data.most_detailed_loaded_mip].HandleCPU();
	}
}


void TextureStreamer::PostTimestamp( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp end_timestamp )
{
	NOTIMPL;
}


void TextureStreamer::LoadEverythingBeforeTimestamp( GPUTaskQueue::Timestamp timestamp )
{
	NOTIMPL;
}


TextureStreamer::MemoryMappedFile::~MemoryMappedFile()
{
	Close();
}


bool TextureStreamer::MemoryMappedFile::Open( const std::string_view& path ) noexcept
{
	// be careful with return value for failed winapi functions!
	// different functions return different values for handles!
	if ( IsOpened() )
		return false;

	m_file_handle = CreateFileA( path.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
	if ( m_file_handle == INVALID_HANDLE_VALUE )
		return false;

	LARGE_INTEGER filesize;
	if ( ! GetFileSizeEx( m_file_handle, &filesize ) )
		return false;
	if ( filesize.QuadPart == 0 )
		return false;
	m_file_mapping = CreateFileMappingA( m_file_handle, nullptr, PAGE_READONLY, 0, 0, NULL );
	if ( ! m_file_mapping )
		return false;

	const uint8_t* mapped_region_start = reinterpret_cast<const uint8_t*>( MapViewOfFile( m_file_mapping, FILE_MAP_READ, 0, 0, 0 ) );
	if ( ! mapped_region_start )
		return false;

	m_mapped_file_data = span<const uint8_t>( mapped_region_start, mapped_region_start + filesize.QuadPart );

	return true;
}


bool TextureStreamer::MemoryMappedFile::IsOpened() const noexcept
{
	return m_file_handle != nullptr && m_file_handle != INVALID_HANDLE_VALUE
		&& m_file_mapping != nullptr && m_file_mapping != INVALID_HANDLE_VALUE
		&& m_mapped_file_data.cbegin() != nullptr;
}


void TextureStreamer::MemoryMappedFile::Close() noexcept
{
	if ( m_mapped_file_data.cbegin() != nullptr )
		UnmapViewOfFile( m_mapped_file_data.cbegin() );

	if ( m_file_mapping != nullptr && m_file_mapping != INVALID_HANDLE_VALUE )
		CloseHandle( m_file_mapping );

	if ( m_file_handle != nullptr && m_file_handle != INVALID_HANDLE_VALUE )
		CloseHandle( m_file_handle );
}
