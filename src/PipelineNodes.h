#pragma once

#include "PipelineResource.h"

#include "DepthOnlyPass.h"
#include "ToneMappingPass.h"
#include "HBAOPass.h"
#include "PSSMGenPass.h"


template<class Pipeline>
class HBAOGeneratorNode : public BaseRenderNode
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

	HBAOGeneratorNode( Pipeline* pipeline, HBAOPass* hbao_pass )
		: m_pass( hbao_pass ), m_pipeline( pipeline )
	{}

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

		m_pass->Draw( ctx, cmd_list );

		CD3DX12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition( storage.resource,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE )
		};

		cmd_list.ResourceBarrier( 1, barriers );

		SSAOTexture_Noisy out{ storage.srv };
		m_pipeline->SetRes( out );
	}

private:
	HBAOPass* m_pass = nullptr;
	Pipeline* m_pipeline = nullptr;
};


template<class Pipeline>
class ToneMapPassNode : public BaseRenderNode
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

	ToneMapPassNode( Pipeline* pipeline, ToneMappingPass* tonemap_pass )
		: m_pass( tonemap_pass ), m_pipeline( pipeline )
	{}

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

		ToneMappingPass::Context ctx;
		ctx.gpu_data = settings.data;
		ctx.frame_rtv = ldr_buffer.rtv;
		ctx.frame_srv = hdr_buffer.srv;
		ctx.ambient_srv = ambient.srv;
		ctx.ssao_srv = ssao.srv;

		cmd_list.RSSetViewports( 1, &screen_constants.viewport );
		cmd_list.RSSetScissorRects( 1, &screen_constants.scissor_rect );
		m_pass->Draw( ctx, cmd_list );

		TonemappedBackbuffer out{ ldr_buffer.resource, ldr_buffer.rtv };
		m_pipeline->SetRes( out );
	}

private:
	ToneMappingPass* m_pass = nullptr;
	Pipeline* m_pipeline = nullptr;
};


template<class Pipeline>
class UIPassNode : public BaseRenderNode
{
public:
	using InputResources = std::tuple
		<
		TonemappedBackbuffer,
		ImGuiFontHeap
		>;

	using OutputResources = std::tuple
		<
		FinalBackbuffer
		>;

	UIPassNode( Pipeline* pipeline )
		: m_pipeline( pipeline )
	{}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override
	{
		TonemappedBackbuffer backbuffer;
		m_pipeline->GetRes( backbuffer );

		ImGuiFontHeap heap;
		m_pipeline->GetRes( heap );

		ImGui_ImplDX12_NewFrame( &cmd_list );
		cmd_list.SetDescriptorHeaps( 1, &heap.heap );
		ImGui_ImplDX12_RenderDrawData( ImGui::GetDrawData() );

		FinalBackbuffer out{ backbuffer.rtv };
		m_pipeline->SetRes( out );
	}

private:
	Pipeline* m_pipeline;
};

