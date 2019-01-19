#pragma once

#include "PipelineResource.h"
#include "Pipeline.h"

#include "ForwardLightingPass.h"


template<class Pipeline>
class ForwardPassNode : public BaseRenderNode
{
public:
	using InputResources = std::tuple
		<
		ShadowMaps,
		ShadowCascade,
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

	ForwardPassNode( Pipeline* pipeline,
					 DXGI_FORMAT direct_format,
					 DXGI_FORMAT ambient_rtv_format,
					 DXGI_FORMAT normal_format,
					 DXGI_FORMAT depth_stencil_format,
					 ID3D12Device& device )
		: m_pass( device ), m_pipeline( pipeline )
	{
		m_renderstates = m_pass.CompileStates( direct_format, ambient_rtv_format, normal_format, depth_stencil_format, device );
	}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override;

private:
	ForwardLightingPass m_pass;
	Pipeline* m_pipeline = nullptr;

	ForwardLightingPass::States m_renderstates;
};


template<class Pipeline>
inline void ForwardPassNode<Pipeline>::Run( ID3D12GraphicsCommandList& cmd_list )
{
	ShadowMaps shadow_maps;
	m_pipeline->GetRes( shadow_maps );
	ShadowCascade shadow_cascade;
	m_pipeline->GetRes( shadow_cascade );
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

	if ( ! ( hdr_color_buffer.rtv.ptr
			 && dsv.dsv.ptr
			 && ambient_buffer.rtv.ptr
			 && ambient_buffer.srv.ptr
			 && normal_buffer.rtv.ptr
			 && normal_buffer.srv.ptr
			 && shadow_cascade.srv.ptr ) )
		throw SnowEngineException( "ShadowPass: some of the input resources are missing" );

	ForwardLightingPass::Context ctx;
	ctx.back_buffer_rtv = hdr_color_buffer.rtv;
	ctx.depth_stencil_view = dsv.dsv;
	ctx.renderitems = scene.items;
	ctx.shadow_map_srv = shadow_maps.srv;
	ctx.pass_cb = pass_cb.pass_cb;
	ctx.ambient_rtv = ambient_buffer.rtv;
	ctx.normals_rtv = normal_buffer.rtv;
	ctx.shadow_cascade_srv = shadow_cascade.srv;

	const float bgr_color[4] = { 0, 0, 0, 0 };
	cmd_list.ClearRenderTargetView( ctx.back_buffer_rtv, bgr_color, 0, nullptr );
	cmd_list.ClearRenderTargetView( ctx.ambient_rtv, bgr_color, 0, nullptr );
	cmd_list.ClearRenderTargetView( ctx.normals_rtv, bgr_color, 0, nullptr );

	cmd_list.RSSetViewports( 1, &view.viewport );
	cmd_list.RSSetScissorRects( 1, &view.scissor_rect );

	m_pass.Begin( m_renderstates.triangle_fill, cmd_list );

	m_pass.Draw( ctx );

	m_pass.End();

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
