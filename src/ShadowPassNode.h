#pragma once

#include "Framegraph.h"

#include "FramegraphResource.h"

#include "DepthOnlyPass.h"


template<class Framegraph>
class ShadowPassNode : public BaseRenderNode<Framegraph>
{
public:
	using OpenRes = std::tuple
		<
		>;
	using WriteRes = std::tuple
		<
		ResourceInState<ShadowMaps, D3D12_RESOURCE_STATE_DEPTH_WRITE>
		>;
	using ReadRes = std::tuple
		<
		ShadowProducers
		>;
	using CloseRes = std::tuple
		<
		>;

	ShadowPassNode( DXGI_FORMAT dsv_format, int bias, ID3D12Device& device )
		: m_pass( device )
	{
		m_state = m_pass.BuildRenderState( dsv_format, bias, false, true, device );
	}

	virtual void Run( Framegraph& framegraph, ID3D12GraphicsCommandList& cmd_list ) override
	{
		auto& lights_with_shadow = framegraph.GetRes<ShadowProducers>();
		if ( ! lights_with_shadow )
			return;

		auto& shadow_maps = framegraph.GetRes<ShadowMaps>();
		if ( ! shadow_maps )
			throw SnowEngineException( "missing resource" );

		m_pass.Begin( m_state, cmd_list );

		cmd_list.ClearDepthStencilView( shadow_maps->dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );

		for ( const auto& producer : lights_with_shadow->arr )
		{
			cmd_list.RSSetViewports( 1, &producer.map_data.viewport );
			D3D12_RECT sm_scissor;
			{
				sm_scissor.bottom = producer.map_data.viewport.Height + producer.map_data.viewport.TopLeftY;
				sm_scissor.left = producer.map_data.viewport.TopLeftX;
				sm_scissor.right = producer.map_data.viewport.Width + producer.map_data.viewport.TopLeftX; //-V537
				sm_scissor.top = producer.map_data.viewport.TopLeftY;
			}
			cmd_list.RSSetScissorRects( 1, &sm_scissor );

			DepthOnlyPass::Context ctx;
			{
				ctx.depth_stencil_view = shadow_maps->dsv;
				ctx.pass_cbv = producer.map_data.pass_cb;
				ctx.renderitems = make_span( producer.casters );
			}
			m_pass.Draw( ctx );
		}

		m_pass.End();

		cmd_list.ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( shadow_maps->res,
																			D3D12_RESOURCE_STATE_DEPTH_WRITE,
																			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ) );
	}

private:
	DepthOnlyPass m_pass;
	DepthOnlyPass::RenderStateID m_state;
	
};