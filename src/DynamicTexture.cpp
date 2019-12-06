// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "DynamicTexture.h"

#include "CGUtils.h"

DynamicTexture::DynamicTexture(
    std::wstring name,
    uint32_t width, uint32_t height, DXGI_FORMAT format, bool has_mips,
    const ViewsToCreate& views_to_create,
    ID3D12Device& device, DescriptorTableBakery* dtb,
    StagingDescriptorHeap* rtv_heap, StagingDescriptorHeap* dsv_heap,
    const D3D12_CLEAR_VALUE* optimized_clear_value )
    : m_name( std::move( name ) )
{
    m_has_mips = has_mips;
    m_views_to_create = views_to_create;
    if ( optimized_clear_value )
        m_optimized_clear_value = *optimized_clear_value;

    if ( !RecreateTexture( format, width, height, device, dtb, rtv_heap, dsv_heap ) )
        throw SnowEngineException( "Could not create dynamic texture!" );

    m_gpu_res->SetName( m_name.c_str() );
}


bool DynamicTexture::Resize(
    uint32_t width, uint32_t height,
    ID3D12Device& device, DescriptorTableBakery* dtb,
    StagingDescriptorHeap* rtv_heap, StagingDescriptorHeap* dsv_heap )
{
    assert( m_gpu_res );

    DXGI_FORMAT format = m_gpu_res->GetDesc().Format;

    m_rtv.clear();
    m_dsv.clear();
    if ( m_table != DescriptorTableID::nullid )
    {
        assert( dtb );
        dtb->EraseTable( m_table );
        m_table = DescriptorTableID::nullid;
    }
    m_gpu_res = nullptr;

    if ( ! RecreateTexture( format, width, height, device, dtb, rtv_heap, dsv_heap ) )
        return false;

    m_gpu_res->SetName( m_name.c_str() );
    return true;
}


int DynamicTexture::GetMipCount() const
{
    if ( !m_has_mips )
        return 1;

    return int( m_gpu_res->GetDesc().MipLevels );
}


std::optional<DynamicTexture::TextureView> DynamicTexture::GetSrvPerMip() const
{
    if ( m_views_to_create.srv_per_mip )
        return TextureView{ m_table, m_srv_per_mip_offset };
    return std::nullopt;
}


std::optional<DynamicTexture::TextureView> DynamicTexture::GetSrvMain() const
{
    if ( m_views_to_create.srv_main )
        return TextureView{ m_table, m_srv_main_offset };
    return std::nullopt;
}


std::optional<DynamicTexture::TextureView> DynamicTexture::GetSrvCumulative() const
{
    if ( m_views_to_create.srv_cumulative )
        return TextureView{ m_table, m_srv_cumulative_offset };
    return std::nullopt;
}


std::optional<DynamicTexture::TextureView> DynamicTexture::GetUavPerMip() const
{
    if ( m_views_to_create.uav_per_mip )
        return TextureView{ m_table, m_uav_per_mip_offset };
    return std::nullopt;
}


span<const Descriptor> DynamicTexture::GetRtvPerMip() const
{
    return make_span(m_rtv);
}


span<const Descriptor> DynamicTexture::GetDsvPerMip() const
{
    return make_span(m_dsv);
}


ID3D12Resource* DynamicTexture::GetResource() const
{
    return m_gpu_res.Get();
}


DXGI_FORMAT DynamicTexture::GetFormat() const
{
    assert( m_gpu_res );
    return m_gpu_res->GetDesc().Format;
}


bool DynamicTexture::RecreateTexture(
    DXGI_FORMAT format, uint32_t width, uint32_t height,
    ID3D12Device& device, DescriptorTableBakery* dtb,
    StagingDescriptorHeap* rtv_heap, StagingDescriptorHeap* dsv_heap )
{
    assert( m_table == DescriptorTableID::nullid );
    assert( m_dsv.empty() );
    assert( m_rtv.empty() );

    const uint32_t mip_number = m_has_mips ? CalcMipNumber( std::max( width, height ) ) : 1;

    uint32_t total_srv_uav_descriptors = 0;
    m_srv_cumulative_offset = 0;
    m_srv_main_offset = 0;
    m_srv_per_mip_offset = 0;
    m_uav_per_mip_offset = 0;

    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if ( m_views_to_create.dsv_per_mip )
    {
        assert( dsv_heap );
        flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        m_dsv.reserve( mip_number );
    }
    if ( m_views_to_create.rtv_per_mip )
    {
        assert( rtv_heap );
        flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        m_rtv.reserve( mip_number );
    }
    if ( m_views_to_create.uav_per_mip )
    {
        assert( dtb );
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        m_uav_per_mip_offset = 0;
        m_srv_per_mip_offset += mip_number;
        m_srv_cumulative_offset += mip_number;
        m_srv_main_offset += mip_number;
        total_srv_uav_descriptors += mip_number;
    }
    if ( ! ( m_views_to_create.srv_cumulative
             || m_views_to_create.srv_main
             || m_views_to_create.srv_per_mip ) )
    {
        flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }
    else
    {
        assert( dtb );
        if ( m_views_to_create.srv_main )
        {
            total_srv_uav_descriptors += 1;
            m_srv_per_mip_offset += 1;
            m_srv_cumulative_offset += 1;
        }
        if ( m_views_to_create.srv_cumulative )
        {
            total_srv_uav_descriptors += mip_number;
            m_srv_per_mip_offset += mip_number;
        }
        if ( m_views_to_create.srv_per_mip )
        {
            total_srv_uav_descriptors += mip_number;
        }
    }

    const auto* opt_clear_val_ptr = m_optimized_clear_value ? &m_optimized_clear_value.value() : nullptr;

    auto tex_desc = CD3DX12_RESOURCE_DESC::Tex2D( format, width, height, 1, mip_number, 1, 0, flags );
    if ( S_OK != device.CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &tex_desc,
        D3D12_RESOURCE_STATE_COMMON,
        opt_clear_val_ptr,
        IID_PPV_ARGS( m_gpu_res.GetAddressOf() )
    ) )
        return false;

    if ( total_srv_uav_descriptors > 0 )
    {
        m_table = dtb->AllocateTable( total_srv_uav_descriptors );
        if ( m_table == DescriptorTableID::nullid )
            throw SnowEngineException( "could not create descriptor table for a texture" );
    }

    if ( m_table != DescriptorTableID::nullid )
    {
        auto table_start = dtb->ModifyTable( m_table );
        if ( m_views_to_create.srv_main )
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Format = m_views_to_create.srv_main.value();
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = mip_number;
            srv_desc.Texture2D.MostDetailedMip = 0;
            srv_desc.Texture2D.PlaneSlice = 0;
            srv_desc.Texture2D.ResourceMinLODClamp = 0;
            device.CreateShaderResourceView(
                m_gpu_res.Get(), &srv_desc,
                CD3DX12_CPU_DESCRIPTOR_HANDLE( *table_start, m_srv_main_offset, dtb->GetDescriptorIncrementSize() )
            );
        }
        if ( m_views_to_create.srv_per_mip )
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Format = m_views_to_create.srv_per_mip.value();
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = 1;
            srv_desc.Texture2D.PlaneSlice = 0;
            for ( uint32_t i = 0; i < mip_number; ++i )
            {
                srv_desc.Texture2D.MostDetailedMip = i;
                srv_desc.Texture2D.ResourceMinLODClamp = float(i);
                device.CreateShaderResourceView(
                    m_gpu_res.Get(), &srv_desc,
                    CD3DX12_CPU_DESCRIPTOR_HANDLE( *table_start, m_srv_per_mip_offset + i, dtb->GetDescriptorIncrementSize() )
                );
            }
        }
        if ( m_views_to_create.srv_cumulative )
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Format = m_views_to_create.srv_cumulative.value();
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            constexpr UINT use_all_mips_from_most_detailed = -1; // msdn recommends to do this
            srv_desc.Texture2D.MipLevels = use_all_mips_from_most_detailed;
            srv_desc.Texture2D.PlaneSlice = 0;
            for ( uint32_t i = 0; i < mip_number; ++i )
            {
                srv_desc.Texture2D.MostDetailedMip = i;
                srv_desc.Texture2D.ResourceMinLODClamp = float(i);
                device.CreateShaderResourceView(
                    m_gpu_res.Get(), &srv_desc,
                    CD3DX12_CPU_DESCRIPTOR_HANDLE( *table_start, m_srv_cumulative_offset + i, dtb->GetDescriptorIncrementSize() )
                );
            }
        }
        if ( m_views_to_create.uav_per_mip )
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc;
            uav_desc.Format = m_views_to_create.uav_per_mip.value();
            uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uav_desc.Texture2D.PlaneSlice = 0;
            for ( uint32_t i = 0; i < mip_number; ++i )
            {
                uav_desc.Texture2D.MipSlice = i;
                device.CreateUnorderedAccessView(
                    m_gpu_res.Get(), nullptr, &uav_desc,
                    CD3DX12_CPU_DESCRIPTOR_HANDLE( *table_start, m_uav_per_mip_offset + i, dtb->GetDescriptorIncrementSize() )
                );
            }
        }
        if ( m_views_to_create.rtv_per_mip )
        {
            D3D12_RENDER_TARGET_VIEW_DESC rtv_desc;
            rtv_desc.Format = m_views_to_create.rtv_per_mip.value();
            rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            rtv_desc.Texture2D.PlaneSlice = 0;
            for ( uint32_t i = 0; i < mip_number; ++i )
            {
                rtv_desc.Texture2D.MipSlice = i;
                const auto& descriptor = m_rtv.emplace_back( rtv_heap->AllocateDescriptor() );
                device.CreateRenderTargetView(
                    m_gpu_res.Get(), &rtv_desc,
                    descriptor.HandleCPU()
                );
            }
        }
        if ( m_views_to_create.dsv_per_mip )
        {
            D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc;
            dsv_desc.Format = m_views_to_create.dsv_per_mip.value();
            dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
            for ( uint32_t i = 0; i < mip_number; ++i )
            {
                dsv_desc.Texture2D.MipSlice = i;
                const auto& descriptor = m_dsv.emplace_back( dsv_heap->AllocateDescriptor() );
                device.CreateDepthStencilView(
                    m_gpu_res.Get(), &dsv_desc,
                    descriptor.HandleCPU()
                );
            }
        }
    }

    return true;
}
