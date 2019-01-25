#pragma once

#include "PipelineResource.h"
#include "Pipeline.h"

#include "HBAOPass.h"


template<class Pipeline>
class HBAONode : public BaseRenderNode
{
public:
	using OpenRes = std::tuple
		<
		>;
	using WriteRes = std::tuple
		<
		SSAOBuffer_Noisy
		>;
	using ReadRes = std::tuple
		<
		NormalBuffer,
		DepthStencilBuffer,
		ScreenConstants,
		ForwardPassCB,
		HBAOSettings
		>;
	using CloseRes = std::tuple
		<
		>;

	HBAONode( Pipeline* pipeline, DXGI_FORMAT rtv_format, ID3D12Device& device )
		: m_pass( device ), m_pipeline( pipeline )
	{
		m_state = m_pass.BuildRenderState( rtv_format, device );
	}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override
	{
		auto& ssao_buffer = m_pipeline->GetRes<SSAOBuffer_Noisy>();
		if ( ! ssao_buffer )
			throw SnowEngineException( "missing resource" );

		auto& normal_buffer = m_pipeline->GetRes<NormalBuffer>();
		auto& depth_buffer = m_pipeline->GetRes<DepthStencilBuffer>();
		auto& forward_cb = m_pipeline->GetRes<ForwardPassCB>();
		auto& settings = m_pipeline->GetRes<HBAOSettings>();
		if ( ! normal_buffer || ! depth_buffer
			 || ! forward_cb || ! settings )
			throw SnowEngineException( "missing resource" );

		m_pass.Begin( m_state, cmd_list );

		const auto& storage_desc = ssao_buffer->res->GetDesc();
		D3D12_VIEWPORT viewport;
		{
			viewport.TopLeftX = 0;
			viewport.TopLeftY = 0;
			viewport.MinDepth = 0;
			viewport.MaxDepth = 1;
			viewport.Width = storage_desc.Width;
			viewport.Height = storage_desc.Height;
		}
		D3D12_RECT scissor;
		{
			scissor.left = 0;
			scissor.top = 0;
			scissor.right = storage_desc.Width;
			scissor.bottom = storage_desc.Height;
		}

		cmd_list.RSSetViewports( 1, &viewport );
		cmd_list.RSSetScissorRects( 1, &scissor );

		settings->data.render_target_size = DirectX::XMFLOAT2( viewport.Width, viewport.Height );

		HBAOPass::Context ctx;
		ctx.depth_srv = depth_buffer->srv;
		ctx.normals_srv = normal_buffer->srv;
		ctx.pass_cb = forward_cb->pass_cb;
		ctx.ssao_rtv = ssao_buffer->rtv;
		ctx.settings = settings->data;

		m_pass.Draw( ctx );

		CD3DX12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition( ssao_buffer->res,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE )
		};

		cmd_list.ResourceBarrier( 1, barriers );

		m_pass.End();
	}

private:
	HBAOPass m_pass;
	HBAOPass::RenderStateID m_state;
	Pipeline* m_pipeline = nullptr;
};