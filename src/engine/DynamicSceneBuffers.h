#pragma once

#include <wrl.h>
#include <d3d12.h>

#include "SceneSystemData.h"

class DynamicSceneBuffers
{
public:

    DynamicSceneBuffers( Microsoft::WRL::ComPtr<ID3D12Device> device, Scene* scene, size_t num_bufferized_frames ) noexcept;

    void Update();

    void AddMaterial( MaterialID id ) noexcept;

private:
    
    struct BufferData
    {
        size_t offset;
        size_t dirty_buffers_cnt;
    };

    struct MaterialData
    {
        MaterialID id;
        BufferData data;
    };

    struct BufferInstance
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> gpu_res;
        uint8_t* mapped_data = nullptr;
        size_t capacity = 0;
    };

    void InvalidateAllBuffers() noexcept;
    bool NeedToRebuildBuffer() const noexcept;
    BufferInstance& CurrentBuffer() noexcept;

    void RebuildBuffer( BufferInstance& buffer );
    void UpdateBufferContents( BufferInstance& buffer );

    // fnUse_Item is a functor with void( Item_Data&, Item& ) signature
    template<typename fnUseMaterial>
    void UpdateAllItems( const BufferInstance& buffer, const fnUseMaterial& fn_mat );

    MaterialConstants CreateMaterialGPUData( const MaterialPBR& cpu_data ) const noexcept;

    template<typename Data>
    void CopyToBuffer( BufferInstance& buffer, const BufferData& dst, const Data& src ) const noexcept;

    std::vector<MaterialData> m_materials;
    
    std::vector<BufferInstance> m_buffers;
    size_t m_buffer_size = 0;
    size_t m_dirty_buffers_cnt = 0;
    size_t m_cur_buffer_idx = 0;

    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Scene* m_scene;
};
