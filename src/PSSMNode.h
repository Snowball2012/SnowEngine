#pragma once

#include "FramegraphResource.h"

#include "PSSMGenPass.h"


template<class Framegraph>
class PSSMNode : public BaseRenderNode<Framegraph>
{
public:
    using OpenRes = std::tuple
        <
        >;
    using WriteRes = std::tuple
        <
        ResourceInState<ShadowCascade, D3D12_RESOURCE_STATE_DEPTH_WRITE>
        >;
    using ReadRes = std::tuple
        <
        ShadowCascadeProducers,
        ForwardPassCB
        >;
    using CloseRes = std::tuple
        <
        >;

    PSSMNode( DXGI_FORMAT dsv_format, int bias, ID3D12Device& device )
        : m_pass( device )
    {
        m_state = m_pass.BuildRenderState( dsv_format, bias, true, device );
    }

    virtual void Run( Framegraph& framegraph, ID3D12GraphicsCommandList& cmd_list ) override
    {
        OPTICK_EVENT();
        auto& lights_with_pssm = framegraph.GetRes<ShadowCascadeProducers>();
        if ( ! lights_with_pssm )
            return;

        auto& shadow_cascade = framegraph.GetRes<ShadowCascade>();
        if ( ! shadow_cascade )
            throw SnowEngineException( "missing resource" );

        auto& pass_cb = framegraph.GetRes<ForwardPassCB>();
        if ( ! pass_cb )
            throw SnowEngineException( "missing resource" );
        
        PIXBeginEvent( &cmd_list, PIX_COLOR( 200, 210, 230 ), "PSSMGenPass" );

        m_pass.Begin( m_state, cmd_list );

		for ( const auto& dsv : shadow_cascade->dsvs )
			cmd_list.ClearDepthStencilView( dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );

        for ( const auto& producer : lights_with_pssm->arr )
        {
            cmd_list.RSSetViewports( 1, &producer.viewport );
            D3D12_RECT sm_scissor;
            {
                sm_scissor.bottom = producer.viewport.Height + producer.viewport.TopLeftY;
                sm_scissor.left = producer.viewport.TopLeftX;
                sm_scissor.right = producer.viewport.Width + producer.viewport.TopLeftX; //-V537
                sm_scissor.top = producer.viewport.TopLeftY;
            }
            cmd_list.RSSetScissorRects( 1, &sm_scissor );

            const auto caster_span = make_span( producer.casters );
            PSSMGenPass::Context ctx;
            {
                ctx.depth_stencil_views = shadow_cascade->dsvs;
                ctx.pass_cbv = pass_cb->pass_cb;
                ctx.renderitems = make_single_elem_span( caster_span );
                ctx.light_idx = producer.light_idx_in_cb;
            }
            m_pass.Draw( ctx );
        }

        m_pass.End();

        PIXEndEvent( &cmd_list );
    }

private:
    PSSMGenPass m_pass;
    PSSMGenPass::RenderStateID m_state;
    
};