#pragma once

#include "Ptr.h"
#include "DescriptorTableBakery.h"
#include "StagingDescriptorHeap.h"

#include <d3d12.h>

#include <stdint.h>

class DynamicTexture
{
public:

    struct ViewsToCreate
    {
        std::optional<DXGI_FORMAT> srv_per_mip;
        std::optional<DXGI_FORMAT> srv_cumulative;
        std::optional<DXGI_FORMAT> srv_main;
        std::optional<DXGI_FORMAT> uav_per_mip;
        std::optional<DXGI_FORMAT> rtv_per_mip;
        std::optional<DXGI_FORMAT> dsv_per_mip;
    };

    DynamicTexture(
        uint32_t width, uint32_t height, DXGI_FORMAT format, bool has_mips,
        const ViewsToCreate& views_to_create,
        ID3D12Device& device, DescriptorTableBakery* dtb,
        StagingDescriptorHeap* rtv_heap,
        StagingDescriptorHeap* dsv_heap,
        const D3D12_CLEAR_VALUE* optimized_clear_value
    ); // dtb, rtv_heap or dsv_heap may be nullptr if the texture doesn't have correspondiong views
    
    // no gpu access is allowed for the texture when you call this method
    bool Resize(
        uint32_t width, uint32_t height,
        ID3D12Device& device, DescriptorTableBakery* dtb,
        StagingDescriptorHeap* rtv_heap,
        StagingDescriptorHeap* dsv_heap
    ); // dtb, rtv_heap or dsv_heap may be nullptr if the texture doesn't have correspondiong views

    int GetMipCount() const;

    struct TextureView
    {
        DescriptorTableID table;
        int table_offset;
    };
    std::optional<TextureView> GetSrvPerMip() const;
    std::optional<TextureView> GetSrvMain() const;
    std::optional<TextureView> GetSrvCumulative() const;
    
    std::optional<TextureView> GetUavPerMip() const;

    span<const Descriptor> GetRtvPerMip() const;
    span<const Descriptor> GetDsvPerMip() const;

private:

    ComPtr<ID3D12Resource> m_gpu_res;

    bool m_has_mips;
    ViewsToCreate m_views_to_create;

    int m_srv_per_mip_offset;
    int m_srv_cumulative_offset;
    int m_srv_main_offset;
    int m_uav_per_mip_offset;
    DescriptorTableID m_table;
    bc::small_vector<Descriptor, 10> m_rtv;
    bc::small_vector<Descriptor, 10> m_dsv;
};