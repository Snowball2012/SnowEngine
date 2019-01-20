#pragma once

#include "Pipeline.h"

#include "PipelineResource.h"

#include "DepthOnlyPass.h"


template<class Pipeline>
class ShadowPassNode : public BaseRenderNode
{
public:
	using InputResources = std::tuple
		<
		ShadowProducers,
		ShadowMapStorage
		>;

	using OutputResources = std::tuple
		<
		ShadowMaps
		>;

	ShadowPassNode( Pipeline* pipeline, DXGI_FORMAT dsv_format, int bias, ID3D12Device& device )
		: m_pass( device ), m_pipeline( pipeline )
	{
		m_state = m_pass.BuildRenderState( dsv_format, bias, false, true, device );
	}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override
	{
		ShadowProducers lights_with_shadow;
		ShadowMapStorage shadow_maps_to_fill;

		m_pipeline->GetRes( lights_with_shadow );
		m_pipeline->GetRes( shadow_maps_to_fill );

		if ( ! shadow_maps_to_fill.res )
			throw SnowEngineException( "ShadowPass: some of the input resources are missing" );

		m_pass.Begin( m_state, cmd_list );

		cmd_list.ClearDepthStencilView( shadow_maps_to_fill.dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );

		for ( const auto& producer : lights_with_shadow.arr )
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
				ctx.depth_stencil_view = shadow_maps_to_fill.dsv;
				ctx.pass_cbv = producer.map_data.pass_cb;
				ctx.renderitems = make_span( producer.casters );
			}
			m_pass.Draw( ctx );
		}

		m_pass.End();

		ShadowMaps output;
		output.srv = shadow_maps_to_fill.srv;
		m_pipeline->SetRes( output );

		cmd_list.ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( shadow_maps_to_fill.res,
																			D3D12_RESOURCE_STATE_DEPTH_WRITE,
																			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ) );
	}

private:
	DepthOnlyPass m_pass;
	DepthOnlyPass::RenderStateID m_state;
	Pipeline* m_pipeline = nullptr;
};