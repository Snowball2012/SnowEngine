#pragma once

#include "FramegraphResource.h"

#include "LightComposePass.h"

template<class Framegraph>
class LightComposeNode : public BaseRenderNode<Framegraph>
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
        ResourceInState<AmbientBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE>,
        ResourceInState<SSAOTexture_Blurred, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE>,
        ScreenConstants
        >;
    using CloseRes = std::tuple
        <
        >;

    LightComposeNode( DXGI_FORMAT rtv_format, ID3D12Device& device )
        : m_pass( device )
    {
        m_state = m_pass.BuildRenderState( rtv_format, device );
    }

    virtual void Run( Framegraph& framegraph, ID3D12GraphicsCommandList& cmd_list ) override
    {
        OPTICK_EVENT();
        auto& hdr_buffer = framegraph.GetRes<HDRBuffer>();
        auto& ambient_buffer = framegraph.GetRes<AmbientBuffer>();
        auto& ssao = framegraph.GetRes<SSAOTexture_Blurred>();
        auto& screen_constants = framegraph.GetRes<ScreenConstants>();

        if ( ! hdr_buffer || ! ambient_buffer
             || ! ssao || ! screen_constants )
            throw SnowEngineException( "missing resource" );

        m_pass.Begin( m_state, cmd_list );

        LightComposePass::Context ctx;
        ctx.frame_rtv = hdr_buffer->rtv;
        ctx.ambient_srv = ambient_buffer->srv;
        ctx.ssao_srv = ssao->srv;

        cmd_list.RSSetViewports( 1, &screen_constants->viewport );
        cmd_list.RSSetScissorRects( 1, &screen_constants->scissor_rect );
        m_pass.Draw( ctx );

        m_pass.End();
    }

private:
    LightComposePass m_pass;
    LightComposePass::RenderStateID m_state;    
};