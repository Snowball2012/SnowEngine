#pragma once

#include "PipelineResource.h"

#include "DepthOnlyPass.h"
#include "ForwardLightingPass.h"
#include "ToneMappingPass.h"

template<class Pipeline>
class DepthPrepassNode : public BaseRenderNode
{
public:
	using InputResources = std::tuple
		<
		DepthStorage,
		ScreenConstants,
		SceneContext,
		ObjectConstantBuffer,
		ForwardPassCB
		>;
	using OutputResources = std::tuple
		<
		FinalSceneDepth
		>;

	DepthPrepassNode( Pipeline* pipeline, DepthOnlyPass* pass )
		: m_pass( pass ), m_pipeline( pipeline )
	{}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override
	{
		DepthStorage dsv;
		m_pipeline->GetRes( dsv );
		ScreenConstants view;
		m_pipeline->GetRes( view );
		SceneContext scene;
		m_pipeline->GetRes( scene );
		ObjectConstantBuffer object_cb;
		m_pipeline->GetRes( object_cb );
		ForwardPassCB pass_cb;
		m_pipeline->GetRes( pass_cb );

		DepthOnlyPass::Context ctx;
		{
			ctx.depth_stencil_view = dsv.dsv;
			ctx.object_cb = object_cb.buffer;
			ctx.pass_cbv = pass_cb.pass_cb;
			ctx.renderitems = &scene.scene->renderitems;
		}

		cmd_list.ClearDepthStencilView( ctx.depth_stencil_view, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr );

		cmd_list.RSSetViewports( 1, &view.viewport );
		cmd_list.RSSetScissorRects( 1, &view.scissor_rect );

		m_pass->Draw( ctx, cmd_list );

		FinalSceneDepth out_depth{ dsv.dsv };
		m_pipeline->SetRes( out_depth );
	}

private:
	DepthOnlyPass* m_pass = nullptr;
	Pipeline* m_pipeline = nullptr;
};

template<class Pipeline>
class ForwardPassNode : public BaseRenderNode
{
public:
	using InputResources = std::tuple
		<
		ShadowMaps,
		HDRColorStorage,
		FinalSceneDepth,
		ScreenConstants,
		SceneContext,
		ObjectConstantBuffer,
		ForwardPassCB
		>;

	using OutputResources = std::tuple
		<
		HDRColorOut
		>;

	ForwardPassNode( Pipeline* pipeline, ForwardLightingPass* pass )
		: m_pass( pass ), m_pipeline( pipeline )
	{}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override 
	{
		ShadowMaps shadow_maps;
		m_pipeline->GetRes( shadow_maps );
		HDRColorStorage hdr_color_buffer;
		m_pipeline->GetRes( hdr_color_buffer );
		FinalSceneDepth dsv;
		m_pipeline->GetRes( dsv );
		ScreenConstants view;
		m_pipeline->GetRes( view );
		SceneContext scene;
		m_pipeline->GetRes( scene );
		ObjectConstantBuffer object_cb;
		m_pipeline->GetRes( object_cb );
		ForwardPassCB pass_cb;
		m_pipeline->GetRes( pass_cb );

		if ( ! ( scene.scene
				 && shadow_maps.light_with_sm
				 && hdr_color_buffer.rtv.ptr
				 && shadow_maps.light_with_sm->size() == 1
				 && object_cb.buffer
				 && dsv.dsv.ptr ) )
			throw SnowEngineException( "ShadowPass: some of the input resources are missing" );

		ForwardLightingPass::Context ctx;
		ctx.back_buffer_rtv = hdr_color_buffer.rtv;
		ctx.depth_stencil_view = dsv.dsv;
		ctx.object_cb = object_cb.buffer;
		ctx.scene = scene.scene;
		ctx.shadow_map_srv = ( *shadow_maps.light_with_sm )[0]->shadow_map->srv->HandleGPU();
		ctx.pass_cb = pass_cb.pass_cb;

		const float bgr_color[4] = { 0, 0, 0, 0 };
		cmd_list.ClearRenderTargetView( ctx.back_buffer_rtv, bgr_color, 0, nullptr );

		cmd_list.RSSetViewports( 1, &view.viewport );
		cmd_list.RSSetScissorRects( 1, &view.scissor_rect );

		m_pass->Draw( ctx, false, cmd_list );

		CD3DX12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition( hdr_color_buffer.resource,
												D3D12_RESOURCE_STATE_RENDER_TARGET,
												D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE )
		};
		
		cmd_list.ResourceBarrier( 1, barriers );

		HDRColorOut out_color{ hdr_color_buffer.srv };

		m_pipeline->SetRes( out_color );
	}

private:
	ForwardLightingPass* m_pass = nullptr;
	Pipeline* m_pipeline = nullptr;
};


template<class Pipeline>
class ShadowPassNode : public BaseRenderNode
{
public:
	using InputResources = std::tuple
		<
		ShadowCasters,
		ShadowProducers,
		ShadowMapStorage,
		ObjectConstantBuffer
		>;

	using OutputResources = std::tuple
		<
		ShadowMaps
		>;

	ShadowPassNode( Pipeline* pipeline, DepthOnlyPass* depth_pass )
		: m_pass( depth_pass ), m_pipeline( pipeline )
	{}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override
	{
		ShadowCasters casters;
		ShadowProducers lights_with_shadow;
		ShadowMapStorage shadow_maps_to_fill;
		ObjectConstantBuffer object_cb;

		m_pipeline->GetRes( casters );
		m_pipeline->GetRes( lights_with_shadow );
		m_pipeline->GetRes( shadow_maps_to_fill );
		m_pipeline->GetRes( object_cb );

		if ( ! ( casters.casters
				 && lights_with_shadow.lights
				 && lights_with_shadow.pass_cbs
				 && shadow_maps_to_fill.sm_storage
				 && object_cb.buffer ) )
			throw SnowEngineException( "ShadowPass: some of the input resources are missing" );

		const size_t nproducers = lights_with_shadow.lights->size();
		if ( nproducers != shadow_maps_to_fill.sm_storage->size() || nproducers != lights_with_shadow.pass_cbs->size() )
			throw SnowEngineException( "ShadowPass: number of lights and shadow maps doesn't match" );

		// shadow map viewport and scissor rect
		D3D12_VIEWPORT sm_viewport;
		{
			sm_viewport.TopLeftX = 0;
			sm_viewport.TopLeftY = 0;
			sm_viewport.Height = 4096;
			sm_viewport.Width = 4096;
			sm_viewport.MinDepth = 0;
			sm_viewport.MaxDepth = 1;
		}
		D3D12_RECT sm_scissor;
		{
			sm_scissor.bottom = 4096;
			sm_scissor.left = 0;
			sm_scissor.right = 4096;
			sm_scissor.top = 0;
		}

		cmd_list.RSSetViewports( 1, &sm_viewport );
		cmd_list.RSSetScissorRects( 1, &sm_scissor );

		for ( size_t light_idx = 0; light_idx < nproducers; ++light_idx )
		{
			const auto& light = (*lights_with_shadow.lights)[light_idx];
			const auto& cb = (*lights_with_shadow.pass_cbs)[light_idx];
			auto& shadow_map = (*shadow_maps_to_fill.sm_storage)[light_idx];
			DepthOnlyPass::Context ctx;
			{
				ctx.depth_stencil_view = shadow_map->shadow_map->dsv->HandleCPU();
				ctx.pass_cbv = (*lights_with_shadow.pass_cbs)[light_idx];
				ctx.renderitems = casters.casters;
				ctx.object_cb = object_cb.buffer;
			}
			cmd_list.ClearDepthStencilView( ctx.depth_stencil_view, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );
			m_pass->Draw( ctx, cmd_list );
		}

		ShadowMaps output;
		output.light_with_sm = lights_with_shadow.lights;
		std::vector<CD3DX12_RESOURCE_BARRIER> transitions;
		transitions.reserve( output.light_with_sm->size() );
		for ( auto& sm : *output.light_with_sm )
			transitions.push_back( CD3DX12_RESOURCE_BARRIER::Transition( sm->shadow_map->texture_gpu.Get(),
																		 D3D12_RESOURCE_STATE_DEPTH_WRITE,
																		 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ) );

		cmd_list.ResourceBarrier( UINT( transitions.size() ), transitions.data() );

		m_pipeline->SetRes( output );
	}

private:
	DepthOnlyPass* m_pass = nullptr;
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
		BackbufferStorage
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
		TonemapNodeSettings settings;
		m_pipeline->GetRes( settings );
		BackbufferStorage ldr_buffer;
		m_pipeline->GetRes( ldr_buffer );

		ToneMappingPass::Context ctx;
		ctx.gpu_data = settings.data;
		ctx.frame_rtv = ldr_buffer.rtv;
		ctx.frame_srv = hdr_buffer.srv;

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
		TonemappedBackbuffer
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

		ImGui_ImplDX12_RenderDrawData( ImGui::GetDrawData() );

		FinalBackbuffer out{ backbuffer.rtv };
		m_pipeline->SetRes( out );
	}

private:
	Pipeline* m_pipeline;
};