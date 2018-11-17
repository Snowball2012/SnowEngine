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
	TextureData tex_data;

	tex_data.id = id;

	if ( ! InitMemoryMappedTexture( path, tex_data ) )
		throw SnowEngineException( "failed to load the texture" );

	tex_data.path = std::move( path );

	tex_data.gpu_res.Reset();
	ThrowIfFailed( DirectX::LoadDDSTextureFromMemoryEx( m_device.Get(),
														tex_data.mapped_file_data.cbegin(), tex_data.mapped_file_data.size(), 0,
														D3D12_RESOURCE_FLAG_NONE,
														DirectX::DDS_LOADER_DEFAULT | DirectX::DDS_LOADER_CREATE_RESERVED_RESOURCE,
														tex_data.gpu_res.GetAddressOf(), tex_data.file_mem ) );

	size_t subresource_num = tex_data.file_mem.size();
	const auto& res_desc = tex_data.gpu_res->GetDesc();
	UINT64 virtual_bytes_total = 0;
	tex_data.virtual_mem.nrows.resize( subresource_num );
	tex_data.virtual_mem.row_size.resize( subresource_num );
	m_device->GetCopyableFootprints( &res_desc, 0, tex_data.file_mem.size(), 0,
									 tex_data.virtual_mem.footprints.data(), 
									 tex_data.virtual_mem.nrows.data(),
									 tex_data.virtual_mem.row_size.data(), &virtual_bytes_total );

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

	return true;
}
