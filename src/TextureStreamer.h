#pragma once

#include "SceneSystemData.h"
#include "StagingDescriptorHeap.h"
#include "GPUPagedAllocator.h"

#include <wrl.h>
#include <d3d12.h>

// Streamed texture manager.
// Streams correct mips to scene textures

class TextureStreamer
{
public:
	TextureStreamer( Microsoft::WRL::ComPtr<ID3D12Device> device, Scene* scene ) noexcept;
	~TextureStreamer( );

	void LoadStreamedTexture( TextureID id, std::string path );

	void Update( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp current_timestamp, ID3D12GraphicsCommandList& cmd_list );

	// post a timestamp for given operation. May throw SnowEngineException if there already is a timestamp for this operation
	void PostTimestamp( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp end_timestamp );

	// Mark as loaded every transaction before timestamp.
	// However, this does not mean the transaction has been completed already.
	// Use this method if you are sure this timestamp will be reached before any subsequent operations on any mesh in transaction
	void LoadEverythingBeforeTimestamp( GPUTaskQueue::Timestamp timestamp );

private:

	struct GPUVirtualLayout
	{
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints;
		std::vector<UINT> nrows;
		std::vector<UINT64> row_size;
	};
	using GPUPhysicalLayout = std::vector<GPUPagedAllocator::ChunkID>;
	using FileLayout = std::vector<D3D12_SUBRESOURCE_DATA>;

	struct TextureData
	{
		TextureID id;
		Microsoft::WRL::ComPtr<ID3D12Resource> gpu_res;

		FileLayout file_mem;
		GPUVirtualLayout virtual_mem;
		GPUPhysicalLayout backing_mem;
		std::vector<Descriptor> mip_cumulative_srv; // srv for mip 0 includes all following mips
		uint32_t most_detailed_loaded_mip = 0;// std::numeric_limits<uint32_t>::max();

		// file mapping stuff
		HANDLE file_handle = INVALID_HANDLE_VALUE;
		HANDLE file_mapping = NULL; // different default values because of different error values for corresponding winapi functions
		span<const uint8_t> mapped_file_data;

		std::string path; // mainly for debug purposes
	};

	// File mapping
	bool InitMemoryMappedTexture( const std::string_view& path, TextureData& data );

	Microsoft::WRL::ComPtr<ID3D12Device> m_device;

	std::vector<TextureData> m_loaded_textures;

	StagingDescriptorHeap m_srv_heap;

	Scene* m_scene = nullptr;
};