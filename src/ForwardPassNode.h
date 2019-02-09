#pragma once

#include "FramegraphResource.h"
#include "Framegraph.h"

#include "ForwardLightingPass.h"

#include "DepthPrepassNode.h"


template<class Framegraph>
class ForwardPassNode : public BaseRenderNode
{
public:
	using OpenRes = std::tuple
		<
		>;
	using WriteRes = std::tuple
		<
		HDRBuffer,
		AmbientBuffer,
		NormalBuffer
		>;
	using ReadRes = std::tuple
		<
		ShadowMaps,
		ShadowCascade,
		ScreenConstants,
		MainRenderitems,
		ForwardPassCB,
		DepthStencilBuffer,
		Skybox
		>;
	using CloseRes = std::tuple
		<
		>;

	ForwardPassNode( Framegraph* framegraph,
					 DXGI_FORMAT direct_format,
					 DXGI_FORMAT ambient_rtv_format,
					 DXGI_FORMAT normal_format,
					 DXGI_FORMAT depth_stencil_format,
					 ID3D12Device& device )
		: m_pass( device ), m_framegraph( framegraph )
	{
		m_renderstates = m_pass.CompileStates( direct_format, ambient_rtv_format, normal_format, depth_stencil_format, device );
	}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override;

private:
	ForwardLightingPass m_pass;
	Framegraph* m_framegraph = nullptr;

	ForwardLightingPass::States m_renderstates;
};


template<class Framegraph>
inline void ForwardPassNode<Framegraph>::Run( ID3D12GraphicsCommandList& cmd_list )
{
	auto& shadow_maps = m_framegraph->GetRes<ShadowMaps>();
	if ( ! shadow_maps )
		NOTIMPL;
	auto& csm = m_framegraph->GetRes<ShadowCascade>();
	if ( ! csm )
		NOTIMPL;

	auto& renderitems = m_framegraph->GetRes<MainRenderitems>();
	if ( ! renderitems )
		NOTIMPL;

	auto& pass_cb = m_framegraph->GetRes<ForwardPassCB>();
	auto& view = m_framegraph->GetRes<ScreenConstants>();
	if ( ! pass_cb || ! view )
		throw SnowEngineException( "missing resource" );

	auto& depth_buffer = m_framegraph->GetRes<DepthStencilBuffer>();
	auto& hdr_buffer = m_framegraph->GetRes<HDRBuffer>();
	auto& ambient_buffer = m_framegraph->GetRes<AmbientBuffer>();
	auto& normal_buffer = m_framegraph->GetRes<NormalBuffer>();
	if ( ! depth_buffer || ! hdr_buffer || ! ambient_buffer || ! normal_buffer )
		throw SnowEngineException( "missing resource" );

	auto& skybox = m_framegraph->GetRes<Skybox>();

	ForwardLightingPass::Context ctx;
	ctx.back_buffer_rtv = hdr_buffer->rtv;
	ctx.depth_stencil_view = depth_buffer->dsv;
	ctx.renderitems = renderitems->items;
	ctx.shadow_map_srv = shadow_maps->srv;
	ctx.pass_cb = pass_cb->pass_cb;
	ctx.ambient_rtv = ambient_buffer->rtv;
	ctx.normals_rtv = normal_buffer->rtv;
	ctx.shadow_cascade_srv = csm->srv;
	ctx.ibl.irradiance_map_srv = skybox->srv;
	ctx.ibl.radiance_multiplier = skybox->radiance_factor;
	ctx.ibl.transform = skybox->tf_cbv;

	const float bgr_color[4] = { 0, 0, 0, 0 };
	cmd_list.ClearRenderTargetView( ctx.back_buffer_rtv, bgr_color, 0, nullptr );
	cmd_list.ClearRenderTargetView( ctx.ambient_rtv, bgr_color, 0, nullptr );
	cmd_list.ClearRenderTargetView( ctx.normals_rtv, bgr_color, 0, nullptr );

	cmd_list.RSSetViewports( 1, &view->viewport );
	cmd_list.RSSetScissorRects( 1, &view->scissor_rect );

	m_pass.Begin( m_renderstates.triangle_fill, cmd_list );

	m_pass.Draw( ctx );

	m_pass.End();

	CD3DX12_RESOURCE_BARRIER barriers[] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition( ambient_buffer->res,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ),
		CD3DX12_RESOURCE_BARRIER::Transition( normal_buffer->res,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE )
	};

	cmd_list.ResourceBarrier( 2, barriers );
}
