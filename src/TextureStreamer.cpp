#include "stdafx.h"

#include "SceneManager.h"
#include "TextureStreamer.h"

#include <dxtk12/DDSTextureLoader.h>
#include <dxtk12/DirectXHelpers.h>


TextureStreamer::TextureStreamer( Microsoft::WRL::ComPtr<ID3D12Device> device, uint64_t gpu_mem_budget, uint64_t cpu_mem_budget,
								  uint8_t n_bufferized_frames, Scene* scene )
	: m_device( std::move( device ) ), m_scene( scene )
	, m_srv_heap( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_device )
	, m_n_bufferized_frames( n_bufferized_frames )
{
	ComPtr<ID3D12Heap> heap;
	CD3DX12_HEAP_DESC heap_desc( gpu_mem_budget, CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ) );
	heap_desc.Flags = D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;

	constexpr uint64_t alignment = 1 << 16; // 64kb
	heap_desc.Alignment = alignment;

	ThrowIfFailed( m_device->CreateHeap( &heap_desc, IID_PPV_ARGS( heap.GetAddressOf() ) ) );
	m_gpu_mem = std::make_unique<GPUPagedAllocator>( std::move( heap ) );

	// 64k alignment
	m_upload_buffer = std::make_unique<CircularUploadBuffer>( m_device, cpu_mem_budget, alignment );
}


TextureStreamer::~TextureStreamer()
{
	for ( auto& tex_data : m_loaded_textures )
		tex_data.mip_cumulative_srv.clear();
}


void TextureStreamer::LoadStreamedTexture( TextureID id, std::string path )
{
	StreamedTextureID new_texture_id = m_loaded_textures.emplace();

	auto& tex_data = m_loaded_textures[new_texture_id];

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
	tex_data.virtual_layout.footprints.resize( subresource_num );
	m_device->GetCopyableFootprints( &res_desc, 0, subresource_num, 0,
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

	UINT ntiles_for_resource;
	UINT num_subresource_tilings = subresource_num;
	std::vector<D3D12_SUBRESOURCE_TILING> subresource_tilings_for_nonpacked_mips( num_subresource_tilings );

	m_device->GetResourceTiling( tex_data.gpu_res.Get(),
								 &ntiles_for_resource,
								 &tex_data.tiling.packed_mip_info,
								 &tex_data.tiling.tile_shape_for_nonpacked_mips,
								 &num_subresource_tilings,
								 0,
								 subresource_tilings_for_nonpacked_mips.data() );

	for ( size_t i = 0; i < tex_data.tiling.packed_mip_info.NumStandardMips; ++i )
	{
		tex_data.tiling.nonpacked_tiling.emplace_back();
		auto& subresource_tiling = tex_data.tiling.nonpacked_tiling.back();
		subresource_tiling.data = subresource_tilings_for_nonpacked_mips[i];
	}
}

	
void TextureStreamer::Update( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp current_timestamp, GPUTaskQueue& copy_queue, ID3D12GraphicsCommandList& cmd_list )
{

	for ( const auto& tex_data : m_loaded_textures )
	{
		auto& texture = *m_scene->TryModifyTexture( tex_data.id );
		if ( texture.IsLoaded() )
			texture.ModifyStagingSRV() = tex_data.mip_cumulative_srv[tex_data.most_detailed_loaded_mip].HandleCPU();
	}
}


void TextureStreamer::PostTimestamp( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp end_timestamp )
{
}


void TextureStreamer::LoadEverythingBeforeTimestamp( GPUTaskQueue::Timestamp timestamp )
{
}


TextureStreamer::MemoryMappedFile::~MemoryMappedFile()
{
	Close();
}

TextureStreamer::MemoryMappedFile::MemoryMappedFile( MemoryMappedFile&& other ) noexcept
{
	m_file_handle = other.m_file_handle;
	m_file_mapping = other.m_file_mapping;
	m_mapped_file_data = other.m_mapped_file_data;
	other.m_file_handle = INVALID_HANDLE_VALUE;
	other.m_file_mapping = nullptr;
	other.m_mapped_file_data = span<const uint8_t>();
}

TextureStreamer::MemoryMappedFile& TextureStreamer::MemoryMappedFile::operator=( MemoryMappedFile && other ) noexcept
{
	m_file_handle = other.m_file_handle;
	m_file_mapping = other.m_file_mapping;
	m_mapped_file_data = other.m_mapped_file_data;
	other.m_file_handle = INVALID_HANDLE_VALUE;
	other.m_file_mapping = nullptr;
	other.m_mapped_file_data = span<const uint8_t>();
	return *this;
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
