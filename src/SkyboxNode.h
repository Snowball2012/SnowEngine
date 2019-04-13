#pragma once

#include "FramegraphResource.h"
#include "Framegraph.h"

#include "SkyboxPass.h"

template<class Framegraph>
class SkyboxNode : public BaseRenderNode<Framegraph>
{
public:
    using OpenRes = std::tuple
        <
        >;
    using WriteRes = std::tuple
        <
        ResourceInState<HDRBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET>
        >;
    using ReadRes = std::tuple
        <
        ResourceInState<DepthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_READ>,
        Skybox,
        ForwardPassCB,
        ScreenConstants
        >;
    using CloseRes = std::tuple
        <
        >;

    SkyboxNode( DXGI_FORMAT rtv_format, DXGI_FORMAT dsv_format, ID3D12Device& device )
        : m_pass( device )
    {
        m_state = m_pass.BuildRenderState( rtv_format, dsv_format, device );
    }

    virtual void Run( Framegraph& framegraph, ID3D12GraphicsCommandList& cmd_list ) override
    {
        auto& hdr_buffer = framegraph.GetRes<HDRBuffer>();
        auto& depth_buffer = framegraph.GetRes<DepthStencilBuffer>();
        auto& skybox = framegraph.GetRes<Skybox>();
        auto& forward_cb = framegraph.GetRes<ForwardPassCB>();
        auto& screen_constants = framegraph.GetRes<ScreenConstants>();

        if ( ! hdr_buffer || ! depth_buffer || ! skybox
             || ! forward_cb || ! screen_constants )
            throw SnowEngineException( "missing resource" );

        if ( skybox->srv_skybox.ptr == 0 )
            return;

        m_pass.Begin( m_state, cmd_list );

        SkyboxPass::Context ctx;
        ctx.frame_dsv = depth_buffer->dsv;
        ctx.frame_rtv = hdr_buffer->rtv;
        ctx.skybox_srv = skybox->srv_skybox;
        ctx.skybox_cb = skybox->tf_cbv;
        ctx.pass_cb = forward_cb->pass_cb;
        ctx.radiance_multiplier = skybox->radiance_factor;

        cmd_list.RSSetViewports( 1, &screen_constants->viewport );
        cmd_list.RSSetScissorRects( 1, &screen_constants->scissor_rect );
        m_pass.Draw( ctx );

        m_pass.End();
    }

private:
    SkyboxPass m_pass;
    SkyboxPass::RenderStateID m_state;
    
};