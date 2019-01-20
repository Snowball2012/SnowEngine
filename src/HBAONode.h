#pragma once

#include "PipelineResource.h"
#include "Pipeline.h"

#include "HBAOPass.h"


template<class Pipeline>
class HBAONode : public BaseRenderNode
{
public:
	using InputResources = std::tuple
		<
		SSNormals,
		FinalSceneDepth,
		SSAOStorage,
		ScreenConstants,
		ForwardPassCB,
		HBAOSettings
		>;

	using OutputResources = std::tuple
		<
		SSAOTexture_Noisy
		>;

	HBAONode( Pipeline* pipeline, DXGI_FORMAT rtv_format, ID3D12Device& device )
		: m_pass( device ), m_pipeline( pipeline )
	{
		m_state = m_pass.BuildRenderState( rtv_format, device );
	}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override
	{
		SSNormals normals;
		FinalSceneDepth projected_depth;
		SSAOStorage storage;
		ForwardPassCB pass_cb;
		HBAOSettings settings;
		m_pipeline->GetRes( normals );
		m_pipeline->GetRes( projected_depth );
		m_pipeline->GetRes( storage );
		m_pipeline->GetRes( pass_cb );
		m_pipeline->GetRes( settings );

		m_pass.Begin( m_state, cmd_list );

		const auto& storage_desc = storage.resource->GetDesc();
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

		settings.data.render_target_size = DirectX::XMFLOAT2( viewport.Width, viewport.Height );

		HBAOPass::Context ctx;
		ctx.depth_srv = projected_depth.srv;
		ctx.normals_srv = normals.srv;
		ctx.pass_cb = pass_cb.pass_cb;
		ctx.ssao_rtv = storage.rtv;
		ctx.settings = settings.data;

		m_pass.Draw( ctx );

		CD3DX12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition( storage.resource,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE )
		};

		cmd_list.ResourceBarrier( 1, barriers );

		m_pass.End();

		SSAOTexture_Noisy out{ storage.srv };
		m_pipeline->SetRes( out );
	}

private:
	HBAOPass m_pass;
	HBAOPass::RenderStateID m_state;
	Pipeline* m_pipeline = nullptr;
};