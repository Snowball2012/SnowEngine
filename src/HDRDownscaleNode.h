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
        
        PIXBeginEvent( &cmd_list, PIX_COLOR( 200, 210, 230 ), "HDR Downscale" );
        m_pass.Begin( m_state, cmd_list );

        cmd_list.ResourceBarrier(
            1,
            &CD3DX12_RESOURCE_BARRIER::Transition(
                hdr_buffer.res,
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                0
            )
        );

        DownscalePass::Context ctx;
        ctx.dst_rtv = hdr_buffer->rtv[1];
        ctx.src_srv = hdr_buffer->srv[0];

        cmd_list.RSSetViewports( 1, &screen_constants->viewport );
        cmd_list.RSSetScissorRects( 1, &screen_constants->scissor_rect );
        m_pass.Draw( ctx );

        // TODO: Unneccesary barrier, find a way to remove it ( different states for subresources maybe? )
        cmd_list.ResourceBarrier(
            1,
            &CD3DX12_RESOURCE_BARRIER::Transition(
                hdr_buffer.res,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                0
            )
        );

        m_pass.End();

        PIXEndEvent( &cmd_list );
    }

private:
    DownscalePass m_pass;
    DownscalePass::RenderStateID m_state;    
};