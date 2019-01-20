#pragma once

#include "Pipeline.h"

#include "PipelineResource.h"

#include "DepthOnlyPass.h"

template<class Pipeline>
class DepthPrepassNode : public BaseRenderNode
{
public:
	using InputResources = std::tuple
		<
		DepthStorage,
		ScreenConstants,
		MainRenderitems,
		ForwardPassCB
		>;
	using OutputResources = std::tuple
		<
		FinalSceneDepth
		>;

	DepthPrepassNode( Pipeline* pipeline, DXGI_FORMAT dsv_format, ID3D12Device& device )
		: m_pass( device ), m_pipeline( pipeline )
	{
		m_state = m_pass.BuildRenderState( dsv_format, 0, true, true, device );
	}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override
	{
		DepthStorage dsv;
		m_pipeline->GetRes( dsv );
		ScreenConstants view;
		m_pipeline->GetRes( view );
		MainRenderitems scene;
		m_pipeline->GetRes( scene );
		ForwardPassCB pass_cb;
		m_pipeline->GetRes( pass_cb );

		m_pass.Begin( m_state, cmd_list );

		cmd_list.ClearDepthStencilView( dsv.dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr );

		cmd_list.RSSetViewports( 1, &view.viewport );
		cmd_list.RSSetScissorRects( 1, &view.scissor_rect );

		DepthOnlyPass::Context ctx;
		{
			ctx.depth_stencil_view = dsv.dsv;
			ctx.pass_cbv = pass_cb.pass_cb;
			ctx.renderitems = scene.items;
		}
		m_pass.Draw( ctx );
		m_pass.End();

		FinalSceneDepth out_depth{ dsv.dsv, dsv.srv };
		m_pipeline->SetRes( out_depth );
	}

private:
	DepthOnlyPass m_pass;
	DepthOnlyPass::RenderStateID m_state;
	Pipeline* m_pipeline = nullptr;
};
