#pragma once

#include "PipelineResource.h"
#include "Pipeline.h"

#include "SkyboxPass.h"

template<class Pipeline>
class SkyboxNode : public BaseRenderNode
{
public:
	using InputResources = std::tuple
		<
		HDRDirect,
		Skybox,
		ForwardPassCB,
		FinalSceneDepth,
		ScreenConstants
		>;

	using OutputResources = std::tuple
		<
		HDRColorOut
		>;

	SkyboxNode( Pipeline* pipeline, DXGI_FORMAT rtv_format, DXGI_FORMAT dsv_format, ID3D12Device& device )
		: m_pass( device ), m_pipeline( pipeline )
	{
		m_state = m_pass.BuildRenderState( rtv_format, dsv_format, device );
	}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override
	{
		HDRDirect direct_lighting;
		m_pipeline->GetRes( direct_lighting );

		Skybox skybox;
		m_pipeline->GetRes( skybox );

		ForwardPassCB forward_cb;
		m_pipeline->GetRes( forward_cb );

		ScreenConstants screen_constants;
		m_pipeline->GetRes( screen_constants );

		FinalSceneDepth depth;
		m_pipeline->GetRes( depth );

		m_pass.Begin( m_state, cmd_list );

		SkyboxPass::Context ctx;
		ctx.frame_dsv = depth.dsv;
		ctx.frame_rtv = direct_lighting.rtv;
		ctx.skybox_srv = skybox.srv;
		ctx.pass_cb = forward_cb.pass_cb;

		cmd_list.RSSetViewports( 1, &screen_constants.viewport );
		cmd_list.RSSetScissorRects( 1, &screen_constants.scissor_rect );
		m_pass.Draw( ctx );

		m_pass.End();

		CD3DX12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition( direct_lighting.resource,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE )
		};

		cmd_list.ResourceBarrier( 1, barriers );

		HDRColorOut hdr_buffer;
		hdr_buffer.srv = direct_lighting.srv;
		m_pipeline->SetRes( hdr_buffer );
	}

private:
	SkyboxPass m_pass;
	SkyboxPass::RenderStateID m_state;
	Pipeline* m_pipeline = nullptr;
};