#pragma once

#include "FramegraphResource.h"
#include "Framegraph.h"

#include "SkyboxPass.h"

template<class Framegraph>
class SkyboxNode : public BaseRenderNode
{
public:
	using OpenRes = std::tuple
		<
		>;
	using WriteRes = std::tuple
		<
		HDRBuffer
		>;
	using ReadRes = std::tuple
		<
		DepthStencilBuffer,
		Skybox,
		ForwardPassCB,
		ScreenConstants
		>;
	using CloseRes = std::tuple
		<
		>;

	SkyboxNode( Framegraph* framegraph, DXGI_FORMAT rtv_format, DXGI_FORMAT dsv_format, ID3D12Device& device )
		: m_pass( device ), m_framegraph( framegraph )
	{
		m_state = m_pass.BuildRenderState( rtv_format, dsv_format, device );
	}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override
	{
		auto& hdr_buffer = m_framegraph->GetRes<HDRBuffer>();
		auto& depth_buffer = m_framegraph->GetRes<DepthStencilBuffer>();
		auto& skybox = m_framegraph->GetRes<Skybox>();
		auto& forward_cb = m_framegraph->GetRes<ForwardPassCB>();
		auto& screen_constants = m_framegraph->GetRes<ScreenConstants>();

		if ( ! hdr_buffer || ! depth_buffer || ! skybox
			 || ! forward_cb || ! screen_constants )
			throw SnowEngineException( "missing resource" );

		if ( skybox->srv.ptr == 0 )
		{
			CD3DX12_RESOURCE_BARRIER barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition( hdr_buffer->res,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE )
			};

			cmd_list.ResourceBarrier( 1, barriers );
			return;
		}

		m_pass.Begin( m_state, cmd_list );

		SkyboxPass::Context ctx;
		ctx.frame_dsv = depth_buffer->dsv;
		ctx.frame_rtv = hdr_buffer->rtv;
		ctx.skybox_srv = skybox->srv;
		ctx.skybox_cb = skybox->tf_cbv;
		ctx.pass_cb = forward_cb->pass_cb;
		ctx.radiance_multiplier = skybox->radiance_factor;

		cmd_list.RSSetViewports( 1, &screen_constants->viewport );
		cmd_list.RSSetScissorRects( 1, &screen_constants->scissor_rect );
		m_pass.Draw( ctx );

		m_pass.End();

		CD3DX12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition( hdr_buffer->res,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE )
		};

		cmd_list.ResourceBarrier( 1, barriers );
	}

private:
	SkyboxPass m_pass;
	SkyboxPass::RenderStateID m_state;
	Framegraph* m_framegraph = nullptr;
};