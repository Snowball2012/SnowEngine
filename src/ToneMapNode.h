#pragma once

#include "FramegraphResource.h"
#include "Framegraph.h"

#include "ToneMappingPass.h"

template<class Framegraph>
class ToneMapNode : public BaseRenderNode
{
public:
	using OpenRes = std::tuple
		<
		>;
	using WriteRes = std::tuple
		<
		SDRBuffer
		>;
	using ReadRes = std::tuple
		<
		HDRBuffer,
		AmbientBuffer,
		TonemapNodeSettings,
		SSAOTexture_Blurred,
		ScreenConstants
		>;
	using CloseRes = std::tuple
		<
		>;

	ToneMapNode( Framegraph* framegraph, DXGI_FORMAT rtv_format, ID3D12Device& device )
		: m_pass( device ), m_framegraph( framegraph )
	{
		m_state = m_pass.BuildRenderState( rtv_format, device );
	}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override
	{
		auto& sdr_buffer = m_framegraph->GetRes<SDRBuffer>();
		auto& hdr_buffer = m_framegraph->GetRes<HDRBuffer>();
		auto& ambient_buffer = m_framegraph->GetRes<AmbientBuffer>();
		auto& settings = m_framegraph->GetRes<TonemapNodeSettings>();
		auto& ssao = m_framegraph->GetRes<SSAOTexture_Blurred>();
		auto& screen_constants = m_framegraph->GetRes<ScreenConstants>();

		if ( ! sdr_buffer || ! hdr_buffer || ! ambient_buffer
			 || ! settings || ! ssao || ! screen_constants )
			throw SnowEngineException( "missing resource" );

		m_pass.Begin( m_state, cmd_list );

		ToneMappingPass::Context ctx;
		ctx.gpu_data = settings->data;
		ctx.frame_rtv = sdr_buffer->rtv;
		ctx.frame_srv = hdr_buffer->srv;
		ctx.ambient_srv = ambient_buffer->srv;
		ctx.ssao_srv = ssao->srv;

		cmd_list.RSSetViewports( 1, &screen_constants->viewport );
		cmd_list.RSSetScissorRects( 1, &screen_constants->scissor_rect );
		m_pass.Draw( ctx );

		m_pass.End();
	}

private:
	ToneMappingPass m_pass;
	ToneMappingPass::RenderStateID m_state;
	Framegraph* m_framegraph = nullptr;
};