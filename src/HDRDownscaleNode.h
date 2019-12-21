#pragma once

#include "FramegraphResource.h"

#include "DownscalePass.h"

template<class Framegraph>
class HDRDownscaleNode : public BaseRenderNode<Framegraph>
{
public:
    using OpenRes = std::tuple
        <
        >;
    using WriteRes = std::tuple
        <
        ResourceInState<HDRBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET> // ToDo: first mip in SRV, others in RTV
        >;
    using ReadRes = std::tuple
        <
        ScreenConstants,
        HDRLightingMain
        >;
    using CloseRes = std::tuple
        <
        >;

    HDRDownscaleNode( DXGI_FORMAT rtv_format, ID3D12Device& device )
        : m_pass( device )
    {
        m_state = m_pass.BuildRenderState( rtv_format, device );
    }

    virtual void Run( Framegraph& framegraph, ID3D12GraphicsCommandList& cmd_list ) override
    {
        OPTICK_EVENT();
        auto& hdr_buffer = framegraph.GetRes<HDRBuffer>();

        if ( ! hdr_buffer )
            throw SnowEngineException( "missing resource" );

        assert( hdr_buffer->nmips == hdr_buffer->srv.size()
                && hdr_buffer->nmips == hdr_buffer->rtv.size() );

        auto& screen_constants = framegraph.GetRes<ScreenConstants>();

        if ( ! screen_constants )
            throw SnowEngineException( "missing screen constants" );
        
        PIXBeginEvent( &cmd_list, PIX_COLOR( 200, 210, 230 ), "HDR Downscale" );
        m_pass.Begin( m_state, cmd_list );

        DirectX::XMUINT2 size = hdr_buffer->size;

        for (uint32_t mip = 0; mip + 1 < hdr_buffer->nmips; ++mip)
        {
            size.x = std::max<uint32_t>( size.x / 2, 1 );
            size.y = std::max<uint32_t>( size.y / 2, 1 );

            cmd_list.ResourceBarrier(
                1,
                &CD3DX12_RESOURCE_BARRIER::Transition(
                    hdr_buffer->res,
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    mip
                )
            );

            DownscalePass::Context ctx;
            ctx.dst_rtv = hdr_buffer->rtv[mip + 1];
            ctx.src_srv = hdr_buffer->srv[mip];

            D3D12_VIEWPORT viewport;
            {
                viewport.Width = size.x;
                viewport.Height = size.y;
                viewport.MinDepth = 0;
                viewport.MaxDepth = 1;
                viewport.TopLeftX = viewport.TopLeftY = 0;
            }
            D3D12_RECT scissor;
            {
                scissor.left = scissor.top = 0;
                scissor.right = size.x;
                scissor.bottom = size.y;
            }
            cmd_list.RSSetViewports( 1, &viewport );
            cmd_list.RSSetScissorRects( 1, &scissor );
            m_pass.Draw( ctx );
        }

        // TODO: Unneccesary barrier, find a way to remove it ( different states for subresources maybe? )
        std::vector<D3D12_RESOURCE_BARRIER> barriers;
        barriers.reserve( hdr_buffer->nmips );
        for (uint32_t mip = 0; mip + 1 < hdr_buffer->nmips; ++mip)
        {
            barriers.emplace_back() = CD3DX12_RESOURCE_BARRIER::Transition(
                hdr_buffer->res,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                mip );            
        }

        if ( hdr_buffer->nmips > 1 )
            cmd_list.ResourceBarrier( hdr_buffer->nmips - 1, barriers.data() );

        m_pass.End();

        PIXEndEvent( &cmd_list );
    }

private:
    DownscalePass m_pass;
    DownscalePass::RenderStateID m_state;    
};