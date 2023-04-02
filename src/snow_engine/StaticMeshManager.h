#pragma once

#include <wrl.h>
#include <d3d12.h>

#include "SceneSystemData.h"

// Static mesh loader. No LOD support for now

class StaticMeshManager
{
public:
    StaticMeshManager( Microsoft::WRL::ComPtr<ID3D12Device> device, Scene* scene ) noexcept;

    void LoadStaticMesh( StaticMeshID id, std::string name, const span<const Vertex>& vertices, const span<const uint32_t>& indices );

    void Update( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp current_timestamp, IGraphicsCommandList& cmd_list_copy, IGraphicsCommandList& cmd_list_graphics );

    // post a timestamp for given operation. May throw SnowEngineException if there already is a timestamp for this operation
    void PostTimestamp( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp end_timestamp );

private:
    struct StaticMeshData
    {
        StaticMeshID id;
        Microsoft::WRL::ComPtr<ID3D12Resource> gpu_vb;
        Microsoft::WRL::ComPtr<ID3D12Resource> gpu_ib;
        Microsoft::WRL::ComPtr<ID3D12Resource> gpu_rt_as; // raytracing accel structure
        D3D12_VERTEX_BUFFER_VIEW vbv;
        D3D12_INDEX_BUFFER_VIEW ibv;
        D3D_PRIMITIVE_TOPOLOGY topology;
        std::string name;
    };

    struct UploadTransaction
    {
        std::vector<StaticMeshData> meshes_to_upload;
        std::vector<StaticMeshData> meshes_to_remove;
        std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>,
                              Microsoft::WRL::ComPtr<ID3D12Resource>>> uploaders;
    };


    struct ActiveTransaction
    {
        UploadTransaction transaction;

        SceneCopyOp operation_tag;
        std::optional<GPUTaskQueue::Timestamp> end_timestamp;
    };

    void LoadMeshesFromTransaction( UploadTransaction& transaction, IGraphicsCommandList& cmd_list_graphics );

    std::vector<StaticMeshData> m_loaded_meshes;

    std::vector<ActiveTransaction> m_active_transactions;
    UploadTransaction m_pending_transaction;

    Scene* m_scene = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
};