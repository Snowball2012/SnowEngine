#pragma once

#include "PipelineResource.h"

#include "DepthOnlyPass.h"
#include "ForwardLightingPass.h"
#include "ToneMappingPass.h"
#include "HBAOPass.h"
#include "DepthAwareBlurPass.h"

template<class Pipeline>
class DepthPrepassNode : public BaseRenderNode
{
public:
	using InputResources = std::tuple
		<
		DepthStorage,
		ScreenConstants,
		MainRenderitems,
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
		MainRenderitems scene;
		m_pipeline->GetRes( scene );
		ForwardPassCB pass_cb;
		m_pipeline->GetRes( pass_cb );

		DepthOnlyPass::Context ctx;
		{
			ctx.depth_stencil_view = dsv.dsv;
			ctx.pass_cbv = pass_cb.pass_cb;
			ctx.renderitems = scene.items;
		}

		cmd_list.ClearDepthStencilView( ctx.depth_stencil_view, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr );

		cmd_list.RSSetViewports( 1, &view.viewport );
		cmd_list.RSSetScissorRects( 1, &view.scissor_rect );

		m_pass->Draw( ctx, cmd_list );

		FinalSceneDepth out_depth{ dsv.dsv, dsv.srv };
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
		SSAmbientLightingStorage,
		SSNormalStorage,
		FinalSceneDepth,
		ScreenConstants,
		MainRenderitems,
		ForwardPassCB
		>;

	using OutputResources = std::tuple
		<
		HDRColorOut,
		SSAmbientLighting,
		SSNormals
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
		MainRenderitems scene;
		m_pipeline->GetRes( scene );
		ForwardPassCB pass_cb;
		m_pipeline->GetRes( pass_cb );
		SSAmbientLightingStorage ambient_buffer;
		m_pipeline->GetRes( ambient_buffer );
		SSNormalStorage normal_buffer;
		m_pipeline->GetRes( normal_buffer );

		if ( ! ( scene.items
				 && hdr_color_buffer.rtv.ptr
				 && dsv.dsv.ptr
				 && ambient_buffer.rtv.ptr
				 && ambient_buffer.srv.ptr
				 && normal_buffer.rtv.ptr
				 && normal_buffer.srv.ptr ) )
			throw SnowEngineException( "ShadowPass: some of the input resources are missing" );

		ForwardLightingPass::Context ctx;
		ctx.back_buffer_rtv = hdr_color_buffer.rtv;
		ctx.depth_stencil_view = dsv.dsv;
		ctx.renderitems = scene.items;
		ctx.shadow_map_srv = shadow_maps.srv;
		ctx.pass_cb = pass_cb.pass_cb;
		ctx.ambient_rtv = ambient_buffer.rtv;
		ctx.normals_rtv = normal_buffer.rtv;

		const float bgr_color[4] = { 0, 0, 0, 0 };
		cmd_list.ClearRenderTargetView( ctx.back_buffer_rtv, bgr_color, 0, nullptr );
		cmd_list.ClearRenderTargetView( ctx.ambient_rtv, bgr_color, 0, nullptr );
		cmd_list.ClearRenderTargetView( ctx.normals_rtv, bgr_color, 0, nullptr );

		cmd_list.RSSetViewports( 1, &view.viewport );
		cmd_list.RSSetScissorRects( 1, &view.scissor_rect );

		m_pass->Draw( ctx, false, cmd_list );

		CD3DX12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition( hdr_color_buffer.resource,
												D3D12_RESOURCE_STATE_RENDER_TARGET,
												D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ),
			CD3DX12_RESOURCE_BARRIER::Transition( ambient_buffer.resource,
												D3D12_RESOURCE_STATE_RENDER_TARGET,
												D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ),
			CD3DX12_RESOURCE_BARRIER::Transition( normal_buffer.resource,
												D3D12_RESOURCE_STATE_RENDER_TARGET,
												D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE )
		};

		cmd_list.ResourceBarrier( 3, barriers );

		HDRColorOut out_color{ hdr_color_buffer.srv };
		SSAmbientLighting out_ambient{ ambient_buffer.srv };
		SSNormals out_normals{ normal_buffer.srv };

		m_pipeline->SetRes( out_color );
		m_pipeline->SetRes( out_ambient );
		m_pipeline->SetRes( out_normals );
	}

private:
	ForwardLightingPass* m_pass = nullptr;
	Pipeline* m_pipeline = nullptr;
};


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
class ShadowPassNode : public BaseRenderNode
{
public:
	using InputResources = std::tuple
		<
		ShadowProducers,
		ShadowMapStorage
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
		ShadowProducers lights_with_shadow;
		ShadowMapStorage shadow_maps_to_fill;

		m_pipeline->GetRes( lights_with_shadow );
		m_pipeline->GetRes( shadow_maps_to_fill );

		if ( ! ( lights_with_shadow.arr
				 && shadow_maps_to_fill.res ) )
			throw SnowEngineException( "ShadowPass: some of the input resources are missing" );

		for ( const auto& producer : *lights_with_shadow.arr )
		{
			cmd_list.RSSetViewports( 1, &producer.map_data.viewport );
			D3D12_RECT sm_scissor;
			{
				sm_scissor.bottom = producer.map_data.viewport.Height + producer.map_data.viewport.TopLeftY;
				sm_scissor.left = producer.map_data.viewport.TopLeftX;
				sm_scissor.right = producer.map_data.viewport.Width + producer.map_data.viewport.TopLeftX; //-V537
				sm_scissor.top = producer.map_data.viewport.TopLeftY;
			}
			cmd_list.RSSetScissorRects( 1, &sm_scissor );

			DepthOnlyPass::Context ctx;
			{
				ctx.depth_stencil_view = shadow_maps_to_fill.dsv;
				ctx.pass_cbv = producer.map_data.pass_cb;
				ctx.renderitems = &producer.casters;
			}
			cmd_list.ClearDepthStencilView( ctx.depth_stencil_view, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );
			m_pass->Draw( ctx, cmd_list );
		}

		ShadowMaps output;
		output.srv = shadow_maps_to_fill.srv;

		cmd_list.ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( shadow_maps_to_fill.res,
																			D3D12_RESOURCE_STATE_DEPTH_WRITE,
																			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ) );

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


template<class Pipeline>
class BlurSSAONode : public BaseRenderNode
{
public:
	using InputResources = std::tuple
		<
		SSAOTexture_Noisy,
		FinalSceneDepth,
		SSAOStorage_Blurred,
		ForwardPassCB
		>;

	using OutputResources = std::tuple
		<
		SSAOTexture_Blurred
		>;

	BlurSSAONode( Pipeline* pipeline, DepthAwareBlurPass* blur_pass )
		: m_pass( blur_pass ), m_pipeline( pipeline )
	{}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override
	{
		SSAOTexture_Noisy input;
		FinalSceneDepth depth;
		SSAOStorage_Blurred storage;
		ForwardPassCB pass_cb;
		m_pipeline->GetRes( input );
		m_pipeline->GetRes( depth );
		m_pipeline->GetRes( storage );
		m_pipeline->GetRes( pass_cb );

		DepthAwareBlurPass::Context ctx;
		ctx.depth_srv = depth.srv;
		ctx.input_srv = input.srv;
		ctx.blurred_uav = storage.uav;
		ctx.pass_cb = pass_cb.pass_cb;

		const auto& resource_desc = storage.resource->GetDesc();

		if ( resource_desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D )
			throw SnowEngineException( "destination resource for ssao blur is not a 2d texture" );

		ctx.uav_width = resource_desc.Width;
		ctx.uav_height = resource_desc.Height;

		m_pass->Draw( ctx, cmd_list );

		CD3DX12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition( storage.resource,
			                                      D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			                                      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE )
		};

		cmd_list.ResourceBarrier( 1, barriers );

		SSAOTexture_Blurred out{ storage.srv };
		m_pipeline->SetRes( out );
	}

private:
	DepthAwareBlurPass* m_pass = nullptr;
	Pipeline* m_pipeline = nullptr;
};
