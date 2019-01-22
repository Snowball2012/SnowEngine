#pragma once

#include "Pipeline.h"

#include "PipelineResource.h"

#include "PSSMGenPass.h"


template<class Pipeline>
class PSSMNode : public BaseRenderNode
{
public:
	using InputResources = std::tuple
		<
		ShadowCascadeProducers,
		ShadowCascadeStorage,
		ForwardPassCB
		>;

	using OutputResources = std::tuple
		<
		ShadowCascade
		>;

	PSSMNode( Pipeline* pipeline, DXGI_FORMAT dsv_format, int bias, ID3D12Device& device )
		: m_pass( device ), m_pipeline( pipeline )
	{
		m_state = m_pass.BuildRenderState( dsv_format, bias, true, device );
	}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override
	{
		ShadowCascadeProducers lights_with_pssm;
		ShadowCascadeStorage shadow_cascade_to_fill;
		ForwardPassCB pass_cb;

		m_pipeline->GetRes( lights_with_pssm );
		m_pipeline->GetRes( shadow_cascade_to_fill );
		m_pipeline->GetRes( pass_cb );

		if ( ! shadow_cascade_to_fill.res )
			throw SnowEngineException( "PSSMGenNode: some of the input resources are missing" );

		m_pass.Begin( m_state, cmd_list );

		cmd_list.ClearDepthStencilView( shadow_cascade_to_fill.dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );

		for ( const auto& producer : lights_with_pssm.arr )
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
				ctx.depth_stencil_view = shadow_cascade_to_fill.dsv;
				ctx.pass_cbv = pass_cb.pass_cb;
				ctx.renderitems = make_span( producer.casters );
				ctx.light_idx = producer.light_idx_in_cb;
			}
			m_pass.Draw( ctx );
		}

		m_pass.End();

		ShadowCascade cascade;
		cascade.srv = shadow_cascade_to_fill.srv;

		cmd_list.ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( shadow_cascade_to_fill.res,
																			D3D12_RESOURCE_STATE_DEPTH_WRITE,
																			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ) );

		m_pipeline->SetRes( cascade );
	}

private:
	PSSMGenPass m_pass;
	PSSMGenPass::RenderStateID m_state;
	Pipeline* m_pipeline = nullptr;
};