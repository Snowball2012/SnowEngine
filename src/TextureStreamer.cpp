// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com
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

	tex_data.data_id = new_texture_id;
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
	tex_data.virtual_layout.nrows.resize( subresource_num );
	tex_data.virtual_layout.row_size.resize( subresource_num );
	tex_data.virtual_layout.footprints.resize( subresource_num );
	m_device->GetCopyableFootprints( &res_desc, 0, subresource_num, 0,
									 tex_data.virtual_layout.footprints.data(),
									 tex_data.virtual_layout.nrows.data(),
									 tex_data.virtual_layout.row_size.data(), &tex_data.virtual_layout.total_size );

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
	FinalizeCompletedGPUUploads( current_timestamp );

	CheckFilledUploaders( operation_tag, cmd_list );

	CalcDesiredMipLevels();

	// TODO: drop mips
	UploaderFillData new_upload;
	struct AsyncTask
	{
		span<uint8_t> mapped_uploader;
		std::vector<D3D12_SUBRESOURCE_DATA> src_data;
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> dst_footprints;
		std::vector<UINT> dst_nrows;
		std::vector<UINT64> dst_row_size;
	};
	std::vector<AsyncTask> tasks;
	for ( auto& texture : m_loaded_textures )
	{
		if ( texture.state != TextureState::Normal )
			continue;

		if ( texture.desired_mip < texture.most_detailed_loaded_mip )
		{
			if ( texture.most_detailed_loaded_mip >= texture.mip_cumulative_srv.size() )
			{
				// first load
				uint32_t required_tiles_num = texture.tiling.packed_mip_info.NumTilesForPackedMips;
				uint64_t required_uploader_size = texture.virtual_layout.total_size - texture.virtual_layout.footprints[texture.tiling.packed_mip_info.NumStandardMips].Offset;

				auto uploader_chunk = m_upload_buffer->AllocateBuffer( required_uploader_size );
				if ( ! uploader_chunk.first )
					continue;

				texture.state = TextureState::MipLoading;

				new_upload.mips.emplace_back();
				auto& mip = new_upload.mips.back();
				tasks.emplace_back();
				auto& task = tasks.back();

				mip.id = texture.data_id;
				mip.uploader = uploader_chunk.first;
				task.mapped_uploader = uploader_chunk.second;
				uint32_t start_mip = texture.tiling.packed_mip_info.NumStandardMips;
				task.dst_footprints.assign( texture.virtual_layout.footprints.cbegin() + start_mip, texture.virtual_layout.footprints.cend() );
				task.dst_nrows.assign( texture.virtual_layout.nrows.cbegin() + start_mip, texture.virtual_layout.nrows.cend() );
				task.dst_row_size.assign( texture.virtual_layout.row_size.cbegin() + start_mip, texture.virtual_layout.row_size.cend() );
				task.src_data.assign( texture.file_layout.cbegin() + start_mip, texture.file_layout.cend() );

				// Tile mappings
				texture.tiling.packed_mip_pages = m_gpu_mem->Alloc( required_tiles_num );
				std::vector<D3D12_TILED_RESOURCE_COORDINATE> resource_region_coords;
				resource_region_coords.emplace_back();
				resource_region_coords.back().Subresource = texture.tiling.packed_mip_info.NumStandardMips;
				resource_region_coords.back().X = 0;
				resource_region_coords.back().Y = 0;
				resource_region_coords.back().Z = 0;

				std::vector<D3D12_TILE_REGION_SIZE> resource_region_sizes;
				resource_region_sizes.emplace_back();
				resource_region_sizes.back().UseBox = FALSE;
				resource_region_sizes.back().NumTiles = required_tiles_num;

				std::vector<D3D12_TILE_RANGE_FLAGS> range_flags;
				range_flags.resize( required_tiles_num, D3D12_TILE_RANGE_FLAG_NONE );
				std::vector<UINT> range_start_offsets;
				range_start_offsets.reserve( required_tiles_num );
				for ( uint32_t page : m_gpu_mem->GetPages( texture.tiling.packed_mip_pages ) )
					range_start_offsets.push_back( page );
				std::vector<UINT> range_tile_counts;
				range_tile_counts.resize( required_tiles_num, 1 );
				copy_queue.GetCmdQueue()->UpdateTileMappings( texture.gpu_res.Get(),
															  resource_region_coords.size(),
															  resource_region_coords.data(),
															  resource_region_sizes.data(),
															  m_gpu_mem->GetDXHeap(),
															  range_flags.size(),
															  range_flags.data(),
															  range_start_offsets.data(),
															  range_tile_counts.data(),
															  D3D12_TILE_MAPPING_FLAG_NONE );
			}
		}
	}

	if ( ! tasks.empty() )
	{
		new_upload.op_completed = std::async( std::launch::async, [this, tasks{ move( tasks ) }]() mutable
		{
			for ( AsyncTask& task : tasks )
			{
				for ( size_t i = 0; i < task.dst_footprints.size(); ++i )
				{
					D3D12_MEMCPY_DEST dst;
					dst.pData = task.mapped_uploader.begin() + task.dst_footprints[i].Offset - task.dst_footprints[0].Offset;
					dst.RowPitch = task.dst_footprints[i].Footprint.RowPitch;
					dst.SlicePitch = dst.RowPitch * task.dst_nrows[i];

					MemcpySubresource( &dst, &task.src_data[i], SIZE_T( task.dst_row_size[i] ), task.dst_nrows[i], task.dst_footprints[i].Footprint.Depth );
				}
			}
		} );
		m_uploaders_to_fill.push_back( std::move( new_upload ) );
	}

	for ( const auto& tex_data : m_loaded_textures )
	{
		auto& texture = *m_scene->TryModifyTexture( tex_data.id );
		if ( texture.IsLoaded() )
			texture.ModifyStagingSRV() = tex_data.mip_cumulative_srv[tex_data.most_detailed_loaded_mip].HandleCPU();
	}
}


void TextureStreamer::PostTimestamp( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp end_timestamp )
{
	for ( auto& copy_op : m_active_copy_transactions )
		if ( copy_op.op == operation_tag )
			copy_op.timestamp = end_timestamp;
}


void TextureStreamer::LoadEverythingBeforeTimestamp( GPUTaskQueue::Timestamp timestamp )
{
	FinalizeCompletedGPUUploads( timestamp );
}

TextureStreamer::Stats TextureStreamer::GetPerformanceStats() const noexcept
{
	Stats res;
	res.uploader_mem_allocated = m_upload_buffer->GetHeap()->GetDesc().SizeInBytes;
	res.uploader_mem_in_use = res.uploader_mem_allocated - m_upload_buffer->GetFreeMem();
	res.vidmem_allocated = m_gpu_mem->GetDXHeap()->GetDesc().SizeInBytes;
	res.vidmem_in_use = res.vidmem_allocated - m_gpu_mem->PageSize * m_gpu_mem->GetFreePagesNum();

	return res;
}


void TextureStreamer::FinalizeCompletedGPUUploads( GPUTaskQueue::Timestamp current_timestamp )
{
	size_t first_still_active_transaction = 0;
	for ( ; first_still_active_transaction < m_active_copy_transactions.size(); ++first_still_active_transaction )
	{
		const auto& transaction = m_active_copy_transactions[first_still_active_transaction];
		if ( ( ! transaction.timestamp ) || ( transaction.timestamp > current_timestamp ) )
			break;
	}

	for ( size_t i = 0; i < first_still_active_transaction; ++i )
	{
		for ( auto& mip : m_active_copy_transactions[i].mip )
		{
			TextureData* texture = m_loaded_textures.try_get( mip.id );
			if ( ! texture )
				throw SnowEngineException( "couldn't find the texture for transaction" );
			if ( texture->state != TextureState::MipLoading )
				throw SnowEngineException( "texture state is incorrect for mip loading" );

			// uploader is circular, so be sure we store transaction in allocation order
			m_upload_buffer->DeallocateOldest();
			texture->state = TextureState::Normal;

			const size_t mips_total = texture->mip_cumulative_srv.size();
			if ( texture->most_detailed_loaded_mip >= mips_total )
			{
				// Load packed mip
				texture->most_detailed_loaded_mip = texture->tiling.packed_mip_info.NumStandardMips;
				Texture* scene_texture = m_scene->TryModifyTexture( texture->id );
				if ( scene_texture )
					scene_texture->Load( texture->mip_cumulative_srv[texture->most_detailed_loaded_mip].HandleCPU() );
				// Update() will handle the situation where scene_texture has already been removed
			}
			else
			{
				texture->most_detailed_loaded_mip--;
			}
		}
	}

	if ( first_still_active_transaction > 0 )
	{
		boost::rotate( m_active_copy_transactions, m_active_copy_transactions.begin() + first_still_active_transaction );
		m_active_copy_transactions.erase( m_active_copy_transactions.end() - first_still_active_transaction );
	}
}


void TextureStreamer::CheckFilledUploaders( SceneCopyOp op, ID3D12GraphicsCommandList& cmd_list )
{
	size_t first_still_active_disk_op = 0;
	for ( ; first_still_active_disk_op < m_uploaders_to_fill.size(); ++first_still_active_disk_op )
	{
		const auto& transaction = m_uploaders_to_fill[first_still_active_disk_op];
		if ( transaction.op_completed.wait_for( std::chrono::seconds( 0 ) ) != std::future_status::ready )
			break;
	}

	for ( size_t i = 0; i < first_still_active_disk_op; ++i )
	{
		for ( auto& mip : m_uploaders_to_fill[i].mips )
		{
			TextureData* texture = m_loaded_textures.try_get( mip.id );
			if ( ! texture )
				throw SnowEngineException( "couldn't find the texture for transaction" );
			if ( texture->state != TextureState::MipLoading )
				throw SnowEngineException( "texture state is incorrect for mip loading" );

			const size_t mips_total = texture->mip_cumulative_srv.size();

			if ( texture->most_detailed_loaded_mip >= mips_total )
			{
				// packed mips
				for ( size_t packed_mip_idx = 0; packed_mip_idx < texture->tiling.packed_mip_info.NumPackedMips; ++packed_mip_idx )
				{
					const size_t subresource_idx = packed_mip_idx + texture->tiling.packed_mip_info.NumStandardMips;
					CopyUploaderToMainResource( *texture, mip.uploader, subresource_idx, texture->tiling.packed_mip_info.NumStandardMips, cmd_list );
				}
			}
			else
			{
				// one mip
				const uint32_t mip_idx = texture->most_detailed_loaded_mip - 1;
				CopyUploaderToMainResource( *texture, mip.uploader, mip_idx, mip_idx, cmd_list );
			}
		}

		UploadTransaction copy_to_main_res;
		copy_to_main_res.mip = std::move( m_uploaders_to_fill[i].mips );
		copy_to_main_res.op = op;
		m_active_copy_transactions.push_back( std::move( copy_to_main_res ) );
	}

	if ( first_still_active_disk_op > 0 )
	{
		boost::rotate( m_uploaders_to_fill, m_uploaders_to_fill.begin() + first_still_active_disk_op );
		m_uploaders_to_fill.erase( m_uploaders_to_fill.end() - first_still_active_disk_op );
	}
}


void TextureStreamer::CopyUploaderToMainResource( const TextureData& texture, ID3D12Resource* uploader, uint32_t mip_idx, uint32_t base_mip,
													 ID3D12GraphicsCommandList& cmd_list )
{
	CD3DX12_TEXTURE_COPY_LOCATION dst( texture.gpu_res.Get(), mip_idx );
	CD3DX12_TEXTURE_COPY_LOCATION src( uploader, texture.virtual_layout.footprints[mip_idx] );
	src.PlacedFootprint.Offset -= texture.virtual_layout.footprints[base_mip].Offset;

	cmd_list.CopyTextureRegion( &dst, 0, 0, 0, &src, nullptr );
}


void TextureStreamer::CalcDesiredMipLevels()
{
	// placeholder
	for ( auto& texture : m_loaded_textures )
	{
		if ( texture.state == TextureState::Normal )
			texture.desired_mip = texture.tiling.packed_mip_info.NumStandardMips;
	}
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
