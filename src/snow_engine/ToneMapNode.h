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
        ResourceInState<SDRBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS>
        >;
    using ReadRes = std::tuple
        <
        ResourceInState<HDRBuffer_Final, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE>,
        ResourceInState<HDRBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE>,
        TonemapNodeSettings
        >;
    using CloseRes = std::tuple
        <
        >;

    ToneMapNode( ID3D12Device& device )
        : m_pass( device )
    {
        m_state = m_pass.BuildRenderState( device );
    }

    virtual void Run( Framegraph& framegraph, IGraphicsCommandList& cmd_list ) override
    {
        OPTICK_EVENT();
        auto& sdr_buffer = framegraph.GetRes<SDRBuffer>();
        auto& hdr_buffer = framegraph.GetRes<HDRBuffer_Final>();
        auto& hdr_buffer_pre_taa = framegraph.GetRes<HDRBuffer>();
        auto& settings = framegraph.GetRes<TonemapNodeSettings>();

        if ( ! sdr_buffer || ! hdr_buffer || ! hdr_buffer_pre_taa || ! settings )
            throw SnowEngineException( "missing resource" );
        
        PIXBeginEvent( &cmd_list, PIX_COLOR( 200, 210, 230 ), "Tonemapping" );
        m_pass.Begin( m_state, cmd_list );

        ToneMappingPass::Context ctx;
        ctx.frame_size[0] = uint32_t( sdr_buffer->viewport.Width );
        ctx.frame_size[1] = uint32_t( sdr_buffer->viewport.Height );

        ctx.gpu_data.whitepoint_luminance = settings->whitepoint_luminance;
        ctx.gpu_data.enable_reinhard = settings->enable_reinhard ? 1.0f : 0.0f;
        
        ctx.sdr_uav = sdr_buffer->uav;
        ctx.hdr_srv = hdr_buffer->srv;
        ctx.avg_radiance_srv = hdr_buffer_pre_taa->srv[std::max<uint32_t>( hdr_buffer_pre_taa->nmips - 2, 0 )];

        m_pass.Draw( ctx );

        m_pass.End();
        PIXEndEvent( &cmd_list );
    }

private:
    ToneMappingPass m_pass;
    ToneMappingPass::RenderStateID m_state;
    
};