#pragma once

#include "Ptr.h"
#include "SceneSystemData.h"

#include "DescriptorTableBakery.h"
#include "StagingDescriptorHeap.h"

#include "CubemapConverter.h"

#include <dxtk12/ResourceUploadBatch.h>

// Static texture loader

class CubemapManager
{
public:
    CubemapManager( Microsoft::WRL::ComPtr<ID3D12Device> device, DescriptorTableBakery* descriptor_tables, Scene* scene ) noexcept;
    ~CubemapManager();

    void CreateCubemapFromTexture( CubemapID cubemap_id, TextureID texture_id );

    void LoadCubemap( CubemapID cubemap_id, const std::string& path );

    void Update( GPUTaskQueue::Timestamp current_timestamp, SceneCopyOp copy_op, IGraphicsCommandList& cmd_list );

    // post a timestamp for given operation. May throw SnowEngineException if there already is a timestamp for this operation
    void PostTimestamp( SceneCopyOp operation_tag, GPUTaskQueue::Timestamp end_timestamp );

    void OnBakeDescriptors( IGraphicsCommandList& cmd_list_graphics_queue );

private:
    struct CubemapData
    {
        CubemapID id;
        std::unique_ptr<Descriptor> staging_srv;
        Microsoft::WRL::ComPtr<ID3D12Resource> gpu_res;
    };

    struct ConvertationData
    {
        TextureID texture_id;
        DescriptorTableBakery::TableID texture_srv;
        CubemapData cubemap;
    };

    struct CopyData
    {
        CubemapData cubemap;

        Microsoft::WRL::ComPtr<ID3D12Resource> upload_res;
        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints;

        std::optional<SceneCopyOp> operation_tag;
        std::optional<GPUTaskQueue::Timestamp> end_timestamp;
    };

    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    StagingDescriptorHeap m_srv_heap;

    std::vector<CubemapData> m_loaded_cubemaps;

    std::vector<ConvertationData> m_conversion_in_progress;
    std::vector<CopyData> m_copy_in_progress;

    DescriptorTableBakery* m_desc_tables;

    CubemapConverter m_converter;

    static constexpr uint32_t CubemapResolution = 2048;

    Scene* m_scene = nullptr;

    void FillUploader( CopyData& data, const span<D3D12_SUBRESOURCE_DATA>& subresources );
};