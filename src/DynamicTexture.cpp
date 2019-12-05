#include "stdafx.h"

#include "DynamicTexture.h"

#include "CGUtils.h"

DynamicTexture::DynamicTexture(
    uint32_t width, uint32_t height, DXGI_FORMAT format, bool has_mips,
    const ViewsToCreate& views_to_create,
    ID3D12Device& device, DescriptorTableBakery* dtb,
    StagingDescriptorHeap* rtv_heap, StagingDescriptorHeap* dsv_heap,
    const D3D12_CLEAR_VALUE* optimized_clear_value )
{
    m_has_mips = has_mips;
    m_views_to_create = views_to_create;

    const uint32_t mip_number = has_mips ? CalcMipNumber( std::max( width, height ) ) : 1;

    uint32_t total_srv_uav_descriptors = 0;
    m_srv_cumulative_offset = 0;
    m_srv_main_offset = 0;
    m_srv_per_mip_offset = 0;
    m_uav_per_mip_offset = 0;

    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if ( views_to_create.dsv_per_mip )
    {
        assert( dsv_heap );
        flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        m_dsv.reserve( mip_number );
    }
    if ( views_to_create.rtv_per_mip )
    {
        assert( rtv_heap );
        flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        m_rtv.reserve( mip_number );
    }
    if ( views_to_create.uav_per_mip )
    {
        assert( dtb );
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        m_uav_per_mip_offset = 0;
        m_srv_per_mip_offset += mip_number;
        m_srv_cumulative_offset += mip_number;
        m_srv_main_offset += mip_number;
        total_srv_uav_descriptors += mip_number;
    }
    if ( ! ( views_to_create.srv_cumulative
             || views_to_create.srv_main
             || views_to_create.srv_per_mip ) )
    {
        flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }
    else
    {
        assert( dtb );
        if ( views_to_create.srv_main )
        {
            total_srv_uav_descriptors += 1;
            m_srv_per_mip_offset += 1;
            m_srv_cumulative_offset += 1;
        }
        if ( views_to_create.srv_cumulative )
        {
            total_srv_uav_descriptors += mip_number;
            m_srv_per_mip_offset += mip_number;
        }
        if ( views_to_create.srv_per_mip )
        {
            total_srv_uav_descriptors += mip_number;
        }
    }

    auto tex_desc = CD3DX12_RESOURCE_DESC::Tex2D( format, width, height, 1, has_mips ? 0 : 1, 1, 0, flags );
    ThrowIfFailedH( device.CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &tex_desc,
        D3D12_RESOURCE_STATE_COMMON,
        optimized_clear_value,
        IID_PPV_ARGS( m_gpu_res.GetAddressOf() )
    ) );

    if ( total_srv_uav_descriptors > 0 )
    {
        m_table = dtb->AllocateTable( total_srv_uav_descriptors );
        if ( m_table == DescriptorTableID::nullid )
            throw SnowEngineException( "could not create descriptor table for a texture" );
    }

    NOTIMPL;
    // ToDo: create views
}


bool DynamicTexture::Resize(
    uint32_t width, uint32_t height,
    ID3D12Device& device, DescriptorTableBakery* dtb,
    StagingDescriptorHeap* rtv_heap, StagingDescriptorHeap* dsv_heap )
{
    NOTIMPL;
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
