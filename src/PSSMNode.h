#pragma once

#include "Framegraph.h"

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
		ResourceWithState<ShadowCascade, D3D12_RESOURCE_STATE_DEPTH_WRITE>
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
		auto& lights_with_pssm = framegraph.GetRes<ShadowCascadeProducers>();
		if ( ! lights_with_pssm )
			return;

		auto& shadow_cascade = framegraph.GetRes<ShadowCascade>();
		if ( ! shadow_cascade )
			throw SnowEngineException( "missing resource" );

		auto& pass_cb = framegraph.GetRes<ForwardPassCB>();
		if ( ! pass_cb )
			throw SnowEngineException( "missing resource" );

		m_pass.Begin( m_state, cmd_list );

		cmd_list.ClearDepthStencilView( shadow_cascade->dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );

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

			PSSMGenPass::Context ctx;
			{
				ctx.depth_stencil_view = shadow_cascade->dsv;
				ctx.pass_cbv = pass_cb->pass_cb;
				ctx.renderitems = make_span( producer.casters );
				ctx.light_idx = producer.light_idx_in_cb;
			}
			m_pass.Draw( ctx );
		}

		m_pass.End();

		cmd_list.ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( shadow_cascade->res,
																			D3D12_RESOURCE_STATE_DEPTH_WRITE,
																			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ) );
	}

private:
	PSSMGenPass m_pass;
	PSSMGenPass::RenderStateID m_state;
	
};