#pragma once

#include "GPUTaskQueue.h"

#include <wrl.h>
#include <d3d12.h>

// Static mesh loader. No LOD support for now

class StaticMeshManager
{
public:
	StaticMeshManager( Microsoft::WRL::ComPtr<ID3D12Device> device, Scene* scene ) noexcept;

	void LoadStaticMesh( StaticMeshID id, std::string name, const span<const Vertex>& vertices, const span<const uint32_t>& indices );

	void Update( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp current_timestamp, ID3D12GraphicsCommandList& cmd_list );

	// post a timestamp for given operation. May throw SnowEngineException if there already is a timestamp for this operation
	void PostTimestamp( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp end_timestamp );

	// Mark as loaded every transaction before timestamp.
	// However, this does not mean the transaction is really completed, so uploaders will still remain intact until Update.
	// Use this method if you are sure this timestamp will be reached before any subsequent operations on any mesh in transaction
	void LoadEverythingBeforeTimestamp( GPUTaskQueue::Timestamp timestamp );

private:
	struct StaticMeshData
	{
		StaticMeshID id;
		std::string name;
		Microsoft::WRL::ComPtr<ID3D12Resource> gpu_res;
		D3D12_VERTEX_BUFFER_VIEW vbv;
		D3D12_INDEX_BUFFER_VIEW ibv;
		D3D_PRIMITIVE_TOPOLOGY topology;
	};

	struct UploadTransaction
	{
		std::vector<StaticMeshData> meshes_to_upload;
		std::vector<StaticMeshData> meshes_to_remove;
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> uploaders;
	};


	struct ActiveTransaction
	{
		UploadTransaction transaction;

		SceneCopyOp operation_tag;
		std::optional<GPUTaskQueue::Timestamp> end_timestamp;
	};

	void LoadMeshesFromTransaction( UploadTransaction& transaction );

	std::vector<StaticMeshData> m_loaded_meshes;

	std::vector<ActiveTransaction> m_active_transactions;
	UploadTransaction m_pending_transaction;

	Scene* m_scene = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Device> m_device;
};