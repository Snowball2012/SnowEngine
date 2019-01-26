#pragma once

#include "FramegraphResource.h"
#include "Framegraph.h"

#include "DepthAwareBlurPass.h"


template<class Framegraph>
class BlurSSAONode : public BaseRenderNode
{
public:
	using OpenRes = std::tuple
		<
		>;
	using WriteRes = std::tuple
		<
		SSAOTexture_Blurred,
		SSAOTexture_Transposed
		>;
	using ReadRes = std::tuple
		<
		SSAOBuffer_Noisy,
		DepthStencilBuffer,
		ForwardPassCB
		>;
	using CloseRes = std::tuple
		<
		>;

	BlurSSAONode( Framegraph* framegraph, ID3D12Device& device )
		: m_pass( device ), m_framegraph( framegraph )
	{
		m_state = m_pass.BuildRenderState( device );
	}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override;

private:
	DepthAwareBlurPass m_pass;
	DepthAwareBlurPass::RenderStateID m_state;
	Framegraph* m_framegraph = nullptr;
};


template<class Framegraph>
inline void BlurSSAONode<Framegraph>::Run( ID3D12GraphicsCommandList& cmd_list )
{
	auto& blurred_ssao = m_framegraph->GetRes<SSAOTexture_Blurred>();
	auto& transposed_ssao = m_framegraph->GetRes<SSAOTexture_Transposed>();
	auto& noisy_ssao = m_framegraph->GetRes<SSAOBuffer_Noisy>();
	auto& depth_buffer = m_framegraph->GetRes<DepthStencilBuffer>();
	auto& pass_cb = m_framegraph->GetRes<ForwardPassCB>();

	if ( ! blurred_ssao || ! transposed_ssao || ! noisy_ssao
		 || ! depth_buffer || ! pass_cb )
		throw SnowEngineException( "missing resource" );

	const auto& transposed_desc = transposed_ssao->res->GetDesc();
	const auto& blurred_desc = blurred_ssao->res->GetDesc();

	m_pass.Begin( m_state, cmd_list );

	DepthAwareBlurPass::Context ctx;
	ctx.depth_srv = depth_buffer->srv;
	ctx.input_srv = noisy_ssao->srv;
	ctx.blurred_uav = transposed_ssao->uav;
	ctx.pass_cb = pass_cb->pass_cb;


	ctx.uav_width = transposed_desc.Width;
	ctx.uav_height = transposed_desc.Height;
	ctx.transpose_flag = false;

	m_pass.Draw( ctx );

	CD3DX12_RESOURCE_BARRIER barriers[] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition( transposed_ssao->res,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE )
	};

	cmd_list.ResourceBarrier( 1, barriers );

	ctx.input_srv = transposed_ssao->srv;
	ctx.blurred_uav = blurred_ssao->uav;
	ctx.transpose_flag = true;

	ctx.uav_width = blurred_desc.Width;
	ctx.uav_height = blurred_desc.Height;

	m_pass.Draw( ctx );

	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition( blurred_ssao->res,
														D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
														D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );

	cmd_list.ResourceBarrier( 1, barriers );

	m_pass.End();
}
