#pragma once

#include "PipelineResource.h"

#include "DepthOnlyPass.h"
#include "ForwardLightingPass.h"

template<class Pipeline>
class ForwardPassNode : public BaseRenderNode
{
public:
	using InputResources = std::tuple
		<
		ShadowMaps,
		HDRColorStorage,
		DepthStorage,
		ScreenConstants,
		SceneContext,
		ObjectConstantBuffer
		>;

	using OutputResources = std::tuple
		<
		HDRColorOut,
		FinalSceneDepth
		>;

	ForwardPassNode( Pipeline* pipeline, ForwardLightingPass* pass )
		: m_pass( depth_pass ), m_pipeline( pipeline )
	{}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override 
	{
		ShadowMaps shadow_maps;
		m_pipeline->GetRes( shadow_maps );
		HDRColorStorage hdr_rtv;
		m_pipeline->GetRes( hdr_rtv );
		DepthStorage dsv;
		m_pipeline->GetRes( dsv );
		ScreenConstants view;
		m_pipeline->GetRes( view );
		SceneContext scene;
		m_pipeline->GetRes( scene );
		ObjectConstantBuffer object_cb;
		m_pipeline->GetRes( object_cb );

		if ( ! ( scene.scene
				 && shadow_maps.light_with_sm
				 && hdr_rtv.rtv.ptr
				 && shadow_maps.light_with_sm->size() == 1
				 && object_cb.buffer
				 && dsv.dsv.ptr ) )
			throw SnowEngineException( "ShadowPass: some of the input resources are missing" );

		ForwardLightingPass::Context ctx;
		ctx.back_buffer_rtv = hdr_rtv.rtv;
		ctx.depth_stencil_view = dsv.dsv;
		ctx.object_cb = object_cb.buffer;
		ctx.scene = scene.scene;
		ctx.shadow_map_srv = ( *shadow_maps.light_with_sm )[0].shadow_map->srv;
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
				 && object_cb ) )
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

		cmd_list->RSSetViewports( 1, &sm_viewport );
		cmd_list->RSSetScissorRects( 1, &sm_scissor );

		for ( size_t light_idx = 0; light_idx < nproducers; ++light_idx )
		{
			const auto& light = (*lights_with_shadow.lights)[light_idx];
			const auto& cb = (*lights_with_shadow.pass_cbs)[light_idx];
			auto& shadow_map = (*shadow_maps_to_fill.sm_storage)[light_idx];
			DepthOnlyPass::Context ctx;
			{
				ctx.depth_stencil_view = shadow_map.shadow_map->dsv->HandleCPU();
				ctx.pass_cbv = producer.cb;
				ctx.renderitems = casters.casters;
				ctx.object_cb = object_cb.buffer;
			}
			m_pass->Draw( ctx, cmd_list );
		}

		ShadowMaps output;
		output.light_with_sm = lights_with_shadow.lights;
		std::vector<CD3DX12_RESOURCE_BARRIER> transitions;
		transitions.reserve( output.light_with_sm->size() );
		for ( auto& sm : *output.light_with_sm )
			transitions.push_back( CD3DX12_RESOURCE_BARRIER::Transition( sm.shadow_map->texture_gpu.Get(),
																		 D3D12_RESOURCE_STATE_DEPTH_WRITE,
																		 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ) ) );

		m_sm_cmd_lst->ResourceBarrier( transitions.size(), transitions.data() );

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
		FinalSceneDepth,
		PreviousFrame,
		PreviousFrameStorage,
		TonemapSettings,
		BackbufferStorage
		>;

	using OutputResources = std::tuple
		<
		NewPreviousFrame,
		TonemappedBackbuffer
		>;

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override { std::cout << "Tonemap Pass"; }
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

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override { std::cout << "UI Pass"; }
};