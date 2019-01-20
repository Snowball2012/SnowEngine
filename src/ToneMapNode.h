#pragma once

#include "PipelineResource.h"
#include "Pipeline.h"

#include "ToneMappingPass.h"

template<class Pipeline>
class ToneMapNode : public BaseRenderNode
{
public:
	using InputResources = std::tuple
		<
		HDRColorOut,
		TonemapNodeSettings,
		SSAOTexture_Blurred,
		SSAmbientLighting,
		BackbufferStorage,
		ScreenConstants
		>;

	using OutputResources = std::tuple
		<
		TonemappedBackbuffer
		>;

	ToneMapNode( Pipeline* pipeline, DXGI_FORMAT rtv_format, ID3D12Device& device )
		: m_pass( device ), m_pipeline( pipeline )
	{
		m_state = m_pass.BuildRenderState( rtv_format, device );
	}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override
	{
		HDRColorOut hdr_buffer;
		m_pipeline->GetRes( hdr_buffer );
		SSAmbientLighting ambient;
		m_pipeline->GetRes( ambient );
		SSAOTexture_Blurred ssao;
		m_pipeline->GetRes( ssao );
		TonemapNodeSettings settings;
		m_pipeline->GetRes( settings );
		BackbufferStorage ldr_buffer;
		m_pipeline->GetRes( ldr_buffer );
		ScreenConstants screen_constants;
		m_pipeline->GetRes( screen_constants );

		m_pass.Begin( m_state, cmd_list );

		ToneMappingPass::Context ctx;
		ctx.gpu_data = settings.data;
		ctx.frame_rtv = ldr_buffer.rtv;
		ctx.frame_srv = hdr_buffer.srv;
		ctx.ambient_srv = ambient.srv;
		ctx.ssao_srv = ssao.srv;

		cmd_list.RSSetViewports( 1, &screen_constants.viewport );
		cmd_list.RSSetScissorRects( 1, &screen_constants.scissor_rect );
		m_pass.Draw( ctx );

		m_pass.End();

		TonemappedBackbuffer out{ ldr_buffer.resource, ldr_buffer.rtv };
		m_pipeline->SetRes( out );
	}

private:
	ToneMappingPass m_pass;
	ToneMappingPass::RenderStateID m_state;
	Pipeline* m_pipeline = nullptr;
};