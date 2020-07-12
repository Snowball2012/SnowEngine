#pragma once

#include "FramegraphResource.h"

#include "SingleDownsamplerPass.h"

template<class Framegraph>
class HDRSinglePassDownsampleNode : public BaseRenderNode<Framegraph>
{
public:
    using OpenRes = std::tuple
        <
        >;
    using WriteRes = std::tuple
        <
        ResourceInState<HDRBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS> 
        >;
    using ReadRes = std::tuple
        <
        HDRLightingMain
        >;
    using CloseRes = std::tuple
        <
        ResourceInState<GlobalAtomicBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS>,
        SinglePassDownsamplerShaderCB
        >;

    HDRSinglePassDownsampleNode( ID3D12Device& device )
        : m_pass( device )
    {
        m_state = m_pass.BuildRenderState( device );
    }

    virtual void Run( Framegraph& framegraph, ID3D12GraphicsCommandList& cmd_list ) override
    {
        OPTICK_EVENT();
        auto& hdr_buffer = framegraph.GetRes<HDRBuffer>();
        auto& global_counter = framegraph.GetRes<GlobalAtomicBuffer>();
        auto& cb = framegraph.GetRes<SinglePassDownsamplerShaderCB>();

        if ( ! hdr_buffer || ! global_counter || ! cb )
            throw SnowEngineException( "missing resource" );

        if ( hdr_buffer->nmips > SingleDownsamplerPass::MaxMips )
            throw SnowEngineException( "hdr buffer is too big for a single-pass downsampler" );
        
        PIXBeginEvent( &cmd_list, PIX_COLOR( 200, 210, 230 ), "HDR Single Pass Downscale" );
        m_pass.Begin( m_state, cmd_list );

        DirectX::XMUINT2 size = hdr_buffer->size;

        SingleDownsamplerPass::Context ctx;
        ctx.mip_uav_table = hdr_buffer->uav_per_mip_table;
        ctx.global_atomic_counter_uav = global_counter->uav;
        ctx.shader_cb_gpu = cb->gpu_ptr;
        ctx.shader_cb_mapped = cb->mapped_region;

        ctx.mip_size.resize( hdr_buffer->nmips );
        for ( uint32_t mip = 0; mip < hdr_buffer->nmips; ++mip )
        {
            ctx.mip_size[mip] = size;
            size.x = std::max<uint32_t>( size.x / 2, 1 );
            size.y = std::max<uint32_t>( size.y / 2, 1 );
        }

        m_pass.Draw( ctx );

        // TODO: Unneccesary barrier, find a way to remove it (handle uav barriers in the framegraph automatically)
        std::vector<D3D12_RESOURCE_BARRIER> barriers;
        barriers.reserve( 1 );
        for (uint32_t mip = 0; mip + 1 < hdr_buffer->nmips; ++mip)
        {
            barriers.emplace_back() = CD3DX12_RESOURCE_BARRIER::UAV(
                hdr_buffer->res );            
        }
        
        cmd_list.ResourceBarrier( 1, barriers.data() );

        m_pass.End();

        PIXEndEvent( &cmd_list );
    }

private:
    SingleDownsamplerPass m_pass;
    SingleDownsamplerPass::RenderStateID m_state;    
};