#pragma once

#include "SceneSystemData.h"
#include "StagingDescriptorHeap.h"

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
	struct TextureData
	{
		TextureID id;
		std::unique_ptr<Descriptor> staging_srv;
		Microsoft::WRL::ComPtr<ID3D12Resource> gpu_res;
		std::string path; // mainly for debug purposes

		// File mapping stuff
		HANDLE file_handle = INVALID_HANDLE_VALUE;
		HANDLE file_mapping = NULL; // different default values because of different error values for corresponding winapi functions
		uint64_t file_size = 0;
		const uint8_t* mapped_file_data = nullptr;
	};

	struct UploadData
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints;
	};

	struct UploadTransaction
	{
		std::vector<TextureData> textures_to_upload;
		std::vector<TextureData> textures_to_remove;
		std::vector<UploadData> uploaders;
	};

	struct ActiveTransaction
	{
		UploadTransaction transaction;

		SceneCopyOp operation_tag;
		std::optional<GPUTaskQueue::Timestamp> end_timestamp;
	};

	void LoadTexturesFromTransaction( UploadTransaction& transaction );
	void FinalizeCompletedTransactions( GPUTaskQueue::Timestamp current_timestamp );
	void EnqueueRemovedTexturesForDestruction();
	void UploadTexture( const UploadData& uploader, ID3D12Resource* tex_gpu, ID3D12GraphicsCommandList& cmd_list );

	UploadData CreateAndFillUploader( const Microsoft::WRL::ComPtr<ID3D12Resource>& gpu_res, const span<D3D12_SUBRESOURCE_DATA>& subresources );

	// File mapping
	bool InitMemoryMappedTexture( const std::string_view& path, TextureData& data );

	Microsoft::WRL::ComPtr<ID3D12Device> m_device;

	std::vector<TextureData> m_loaded_textures;

	StagingDescriptorHeap m_srv_heap;

	std::vector<ActiveTransaction> m_active_transactions;
	UploadTransaction m_pending_transaction;

	Scene* m_scene = nullptr;
};