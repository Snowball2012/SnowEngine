#pragma once

#include "FramegraphResource.h"

#include "ToneMappingPass.h"

template<class Framegraph>
class ToneMapNode : public BaseRenderNode<Framegraph>
{
public:
    using OpenRes = std::tuple
        <
        >;
    using WriteRes = std::tuple
        <
        ResourceInState<SDRBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET>
        >;
    using ReadRes = std::tuple
        <
        ResourceInState<HDRBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE>,
        TonemapNodeSettings,
        ScreenConstants
        >;
    using CloseRes = std::tuple
        <
        >;

    ToneMapNode( DXGI_FORMAT rtv_format, ID3D12Device& device )
        : m_pass( device )
    {
        m_state = m_pass.BuildRenderState( rtv_format, device );
    }

    virtual void Run( Framegraph& framegraph, ID3D12GraphicsCommandList& cmd_list ) override
    {
        OPTICK_EVENT();
        auto& sdr_buffer = framegraph.GetRes<SDRBuffer>();
        auto& hdr_buffer = framegraph.GetRes<HDRBuffer>();
        auto& settings = framegraph.GetRes<TonemapNodeSettings>();
        auto& screen_constants = framegraph.GetRes<ScreenConstants>();

        if ( ! sdr_buffer || ! hdr_buffer
             || ! settings || ! screen_constants )
            throw SnowEngineException( "missing resource" );
        
        PIXBeginEvent( &cmd_list, PIX_COLOR( 200, 210, 230 ), "Tonemapping" );
        m_pass.Begin( m_state, cmd_list );

        ToneMappingPass::Context ctx;
        ctx.gpu_data = settings->data;
        ctx.frame_rtv = sdr_buffer->rtv;
        ctx.frame_srv = hdr_buffer->srv_all_mips;

        cmd_list.RSSetViewports( 1, &screen_constants->viewport );
        cmd_list.RSSetScissorRects( 1, &screen_constants->scissor_rect );
        m_pass.Draw( ctx );

        m_pass.End();
        PIXEndEvent( &cmd_list );
    }

private:
    ToneMappingPass m_pass;
    ToneMappingPass::RenderStateID m_state;
    
};