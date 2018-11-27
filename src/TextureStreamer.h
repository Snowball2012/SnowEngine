#pragma once

#include "SceneSystemData.h"
#include "StagingDescriptorHeap.h"
#include "GPUPagedAllocator.h"
#include "CircularUploadBuffer.h"
#include "Ptr.h"

#include <d3d12.h>
#include <future>

// Streamed texture manager.
// Streams correct mips to scene textures

class TextureStreamer
{
public:
	TextureStreamer( Microsoft::WRL::ComPtr<ID3D12Device> device, uint64_t gpu_mem_budget_detailed_mips, uint64_t cpu_mem_budget,
					 uint8_t n_bufferized_frames, Scene* scene );
	~TextureStreamer( );

	void LoadStreamedTexture( TextureID id, std::string path );

	void Update( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp current_timestamp, GPUTaskQueue& copy_queue, ID3D12GraphicsCommandList& cmd_list );

	// post a timestamp for given operation. May throw SnowEngineException if there already is a timestamp for this operation
	void PostTimestamp( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp end_timestamp );

	// Mark as loaded every transaction before timestamp.
	// However, this does not mean the transaction has been completed already.
	// Use this method if you are sure this timestamp will be reached before any subsequent operations on any mesh in transaction
	void LoadEverythingBeforeTimestamp( GPUTaskQueue::Timestamp timestamp );

	struct Stats
	{
		uint64_t vidmem_allocated;
		uint64_t vidmem_in_use;
		uint64_t uploader_mem_allocated;
		uint64_t uploader_mem_in_use;
	};

	Stats GetPerformanceStats() const noexcept;

private:

	struct GPUVirtualLayout
	{
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints;
		std::vector<UINT> nrows;
		std::vector<UINT64> row_size;
		uint64_t total_size = 0;
	};
	using FileLayout = std::vector<D3D12_SUBRESOURCE_DATA>;

	class MemoryMappedFile
	{
	public:
		MemoryMappedFile() = default;
		~MemoryMappedFile();
		MemoryMappedFile( const MemoryMappedFile& other ) = delete;
		MemoryMappedFile( MemoryMappedFile&& other ) noexcept;
		MemoryMappedFile& operator=( MemoryMappedFile&& other ) noexcept;
		bool Open( const std::string_view& path ) noexcept;
		bool IsOpened() const noexcept;
		void Close() noexcept;
		const span<const uint8_t>& GetData() const noexcept { return m_mapped_file_data; }

	private:
		HANDLE m_file_handle = INVALID_HANDLE_VALUE;
		HANDLE m_file_mapping = nullptr; // different default values because of different error values for corresponding winapi functions
		span<const uint8_t> m_mapped_file_data;
	};

	using ChunkID = GPUPagedAllocator::ChunkID;
	struct SubresourceTiling
	{
		D3D12_SUBRESOURCE_TILING data;
		ChunkID mip_pages = ChunkID::nullid;
		uint8_t nframes_in_use = 0;
	};
	struct Tiling
	{
		D3D12_PACKED_MIP_INFO packed_mip_info;
		ChunkID packed_mip_pages = ChunkID::nullid;
		D3D12_TILE_SHAPE tile_shape_for_nonpacked_mips;
		std::vector<SubresourceTiling> nonpacked_tiling;
		uint8_t nframes_in_use = 0;
	};

	enum class TextureState
	{
		Normal,
		MipLoading
	};

	struct TextureData;
	using StreamedTextureID = typename packed_freelist<TextureData>::id;
	struct TextureData
	{
		StreamedTextureID data_id = StreamedTextureID::nullid;
		TextureID id = TextureID::nullid;
		Microsoft::WRL::ComPtr<ID3D12Resource> gpu_res;

		FileLayout file_layout;
		GPUVirtualLayout virtual_layout;
		Tiling tiling;
		std::vector<Descriptor> mip_cumulative_srv; // srv for mip 0 includes all following mips
		uint32_t most_detailed_loaded_mip = std::numeric_limits<uint32_t>::max();
		uint32_t desired_mip = 0;
		TextureState state = TextureState::Normal;

		// file mapping stuff
		MemoryMappedFile file;

		std::string path; // mainly for debug purposes
	};

	using GPUTimestamp = GPUTaskQueue::Timestamp;

	struct MipToLoad
	{
		StreamedTextureID id;
		ID3D12Resource* uploader; // placed resource in upload heap with compatible format
	};
	struct UploaderFillData
	{
		std::vector<MipToLoad> mips;
		std::future<void> op_completed;
	};
	struct UploadTransaction
	{
		std::vector<MipToLoad> mip;
		SceneCopyOp op = std::numeric_limits<SceneCopyOp>::max();
		std::optional<GPUTimestamp> timestamp = std::nullopt;
	};

	void FinalizeCompletedGPUUploads( GPUTaskQueue::Timestamp current_timestamp );
	void CheckFilledUploaders( SceneCopyOp op, ID3D12GraphicsCommandList& cmd_list );
	void CopyUploaderToMainResource( const TextureData& texture, ID3D12Resource* uploader, uint32_t mip_idx, uint32_t base_mip, ID3D12GraphicsCommandList& cmd_list );
	void CalcDesiredMipLevels();
	 
	ComPtr<ID3D12Device> m_device;
	StagingDescriptorHeap m_srv_heap;

	std::unique_ptr<GPUPagedAllocator> m_gpu_mem_basic_mips;
	std::unique_ptr<GPUPagedAllocator> m_gpu_mem_detailed_mips;
	std::unique_ptr<CircularUploadBuffer> m_upload_buffer;

	const uint8_t m_n_bufferized_frames;

	packed_freelist<TextureData> m_loaded_textures;

	std::vector<UploaderFillData> m_uploaders_to_fill;
	std::vector<UploadTransaction> m_active_copy_transactions;

	Scene* m_scene = nullptr;
};