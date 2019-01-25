#pragma once

#include "Pipeline.h"

#include "PipelineResource.h"

#include "DepthOnlyPass.h"

template<class Pipeline>
class DepthPrepassNode : public BaseRenderNode
{
public:
	using OpenRes = std::tuple
		<
		>;
	using WriteRes = std::tuple
		<
		DepthStencilBuffer
		>;
	using ReadRes = std::tuple
		<
		ScreenConstants,
		MainRenderitems,
		ForwardPassCB
		>;
	using CloseRes = std::tuple
		<
		>;

	DepthPrepassNode( Pipeline* pipeline, DXGI_FORMAT dsv_format, ID3D12Device& device )
		: m_pass( device ), m_pipeline( pipeline )
	{
		m_state = m_pass.BuildRenderState( dsv_format, 0, true, true, device );
	}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override
	{
		auto& depth_buffer = m_pipeline->GetRes<DepthStencilBuffer>();
		if ( ! depth_buffer )
			throw SnowEngineException( "missing resource" );

		auto& view = m_pipeline->GetRes<ScreenConstants>();
		if ( ! view )
			throw SnowEngineException( "missing resource" );

		auto& renderitems = m_pipeline->GetRes<MainRenderitems>();
		if ( ! renderitems )
			return;

		auto& pass_cb = m_pipeline->GetRes<ForwardPassCB>();
		if ( ! pass_cb )
			throw SnowEngineException( "missing resource" );

		m_pass.Begin( m_state, cmd_list );

		cmd_list.ClearDepthStencilView( depth_buffer->dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr );

		cmd_list.RSSetViewports( 1, &view->viewport );
		cmd_list.RSSetScissorRects( 1, &view->scissor_rect );

		DepthOnlyPass::Context ctx;
		{
			ctx.depth_stencil_view = depth_buffer->dsv;
			ctx.pass_cbv = pass_cb->pass_cb;
			ctx.renderitems = renderitems->items;
		}
		m_pass.Draw( ctx );
		m_pass.End();
	}

private:
	DepthOnlyPass m_pass;
	DepthOnlyPass::RenderStateID m_state;
	Pipeline* m_pipeline = nullptr;
};
