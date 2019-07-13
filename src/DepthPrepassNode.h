#pragma once

#include "FramegraphResource.h"

#include "DepthOnlyPass.h"

template<class Framegraph>
class DepthPrepassNode : public BaseRenderNode<Framegraph>
{
public:
    using OpenRes = std::tuple
        <
        >;
    using WriteRes = std::tuple
        <
        ResourceInState<DepthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE>
        >;
    using ReadRes = std::tuple
        <
        ScreenConstants,
        MainRenderitems,
        ForwardPassCB
        >;
    using CloseRes = std::tuple
        <
        >;

    DepthPrepassNode( DXGI_FORMAT dsv_format, ID3D12Device& device )
        : m_pass( device )
    {
        m_state = m_pass.BuildRenderState( dsv_format, 0, true, true, device );
    }

    virtual void Run( Framegraph& framegraph, ID3D12GraphicsCommandList& cmd_list ) override
    {
        auto& depth_buffer = framegraph.GetRes<DepthStencilBuffer>();
        if ( ! depth_buffer )
            throw SnowEngineException( "missing resource" );

        auto& view = framegraph.GetRes<ScreenConstants>();
        if ( ! view )
            throw SnowEngineException( "missing resource" );

        auto& renderitems = framegraph.GetRes<MainRenderitems>();
        if ( ! renderitems )
            return;

        auto& pass_cb = framegraph.GetRes<ForwardPassCB>();
        if ( ! pass_cb )
            throw SnowEngineException( "missing resource" );

        m_pass.Begin( m_state, cmd_list );

        cmd_list.ClearDepthStencilView( depth_buffer->dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr );

        cmd_list.RSSetViewports( 1, &view->viewport );
        cmd_list.RSSetScissorRects( 1, &view->scissor_rect );

        DepthOnlyPass::Context ctx;
        {
            ctx.depth_stencil_view = depth_buffer->dsv;
            ctx.pass_cbv = pass_cb->pass_cb;
            ctx.renderitems = renderitems->items;
        }
        m_pass.Draw( ctx );
        m_pass.End();
    }

private:
    DepthOnlyPass m_pass;
    DepthOnlyPass::RenderStateID m_state;
};
