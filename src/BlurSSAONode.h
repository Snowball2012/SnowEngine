#pragma once

#include "PipelineResource.h"
#include "Pipeline.h"

#include "DepthAwareBlurPass.h"


template<class Pipeline>
class BlurSSAONode : public BaseRenderNode
{
public:
	using InputResources = std::tuple
		<
		SSAOTexture_Noisy,
		FinalSceneDepth,
		SSAOStorage_BlurredHorizontal,
		SSAOStorage_Blurred,
		ForwardPassCB
		>;

	using OutputResources = std::tuple
		<
		SSAOTexture_Blurred
		>;

	BlurSSAONode( Pipeline* pipeline, ID3D12Device& device )
		: m_pass( device ), m_pipeline( pipeline )
	{
		m_state = m_pass.BuildRenderState( device );
	}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override;

private:
	DepthAwareBlurPass m_pass;
	DepthAwareBlurPass::RenderStateID m_state;
	Pipeline* m_pipeline = nullptr;
};


template<class Pipeline>
inline void BlurSSAONode<Pipeline>::Run( ID3D12GraphicsCommandList& cmd_list )
{
	SSAOTexture_Noisy input;
	FinalSceneDepth depth;
	SSAOStorage_BlurredHorizontal storage_h;
	SSAOStorage_Blurred storage_v;
	ForwardPassCB pass_cb;
	m_pipeline->GetRes( input );
	m_pipeline->GetRes( depth );
	m_pipeline->GetRes( storage_h );
	m_pipeline->GetRes( storage_v );
	m_pipeline->GetRes( pass_cb );

	m_pass.Begin( m_state, cmd_list );

	DepthAwareBlurPass::Context ctx;
	ctx.depth_srv = depth.srv;
	ctx.input_srv = input.srv;
	ctx.blurred_uav = storage_h.uav;
	ctx.pass_cb = pass_cb.pass_cb;

	const auto& resource_h_desc = storage_h.resource->GetDesc();

	if ( resource_h_desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D )
		throw SnowEngineException( "destination resource for ssao blur is not a 2d texture" );

	ctx.uav_width = resource_h_desc.Width;
	ctx.uav_height = resource_h_desc.Height;
	ctx.transpose_flag = false;

	m_pass.Draw( ctx );

	CD3DX12_RESOURCE_BARRIER barriers[] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition( storage_h.resource,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE )
	};

	cmd_list.ResourceBarrier( 1, barriers );

	ctx.input_srv = storage_h.srv;
	ctx.blurred_uav = storage_v.uav;
	ctx.pass_cb = pass_cb.pass_cb;
	ctx.transpose_flag = true;

	const auto& resource_v_desc = storage_v.resource->GetDesc();

	if ( resource_v_desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D )
		throw SnowEngineException( "destination resource for ssao blur is not a 2d texture" );

	ctx.uav_width = resource_v_desc.Width;
	ctx.uav_height = resource_v_desc.Height;

	m_pass.Draw( ctx );

	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition( storage_v.resource,
														D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
														D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );

	cmd_list.ResourceBarrier( 1, barriers );

	SSAOTexture_Blurred out{ storage_v.srv };
	m_pipeline->SetRes( out );

	m_pass.End();
}
