#pragma once

#include "FramegraphResource.h"

#include "ForwardLightingPass.h"

#include "DepthPrepassNode.h"


template<class Framegraph>
class ForwardPassNode : public BaseRenderNode<Framegraph>
{
public:
    using OpenRes = std::tuple
        <
        >;
    using WriteRes = std::tuple
        <
        ResourceInState<HDRBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET>,
        ResourceInState<AmbientBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET>,
        ResourceInState<NormalBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET>,
        ResourceInState<MotionVectors, D3D12_RESOURCE_STATE_RENDER_TARGET>,
        HDRLightingMain
        >;
    using ReadRes = std::tuple
        <
        ResourceInState<ShadowMaps, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE>,
        ResourceInState<ShadowCascade, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE>,
        ResourceInState<DepthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE>,
        ResourceInState<DirectShadowsMask, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE>,
        ScreenConstants,
        MainRenderitems,
        ForwardPassCB,
        Skybox
        >;
    using CloseRes = std::tuple
        <
        >;

    ForwardPassNode( DXGI_FORMAT direct_format,
                     DXGI_FORMAT ambient_rtv_format,
                     DXGI_FORMAT normal_format,
                     DXGI_FORMAT depth_stencil_format,
                     DXGI_FORMAT motionvectors_format,
                     ID3D12Device& device )
        : m_pass( device )
    {
        m_renderstates = m_pass.CompileStates( direct_format, ambient_rtv_format, normal_format, motionvectors_format, depth_stencil_format, device );
    }

    virtual void Run( Framegraph& framegraph, IGraphicsCommandList& cmd_list ) override;

private:
    ForwardLightingPass m_pass;

    ForwardLightingPass::States m_renderstates;
};


template<class Framegraph>
inline void ForwardPassNode<Framegraph>::Run( Framegraph& framegraph, IGraphicsCommandList& cmd_list )
{
    OPTICK_EVENT();
    auto& shadow_maps = framegraph.GetRes<ShadowMaps>();
    if ( ! shadow_maps )
        NOTIMPL;
    auto& csm = framegraph.GetRes<ShadowCascade>();
    if ( ! csm )
        NOTIMPL;

    bool use_rt_shadows = false;
    auto& rt_shadows = framegraph.GetRes<DirectShadowsMask>();
    if ( rt_shadows )
        use_rt_shadows = true;;

    auto& renderitems = framegraph.GetRes<MainRenderitems>();
    if ( ! renderitems )
        NOTIMPL;

    auto& pass_cb = framegraph.GetRes<ForwardPassCB>();
    auto& view = framegraph.GetRes<ScreenConstants>();
    if ( ! pass_cb || ! view )
        throw SnowEngineException( "missing resource" );

    auto& depth_buffer = framegraph.GetRes<DepthStencilBuffer>();
    auto& hdr_buffer = framegraph.GetRes<HDRBuffer>();
    auto& ambient_buffer = framegraph.GetRes<AmbientBuffer>();
    auto& normal_buffer = framegraph.GetRes<NormalBuffer>();
    auto& motionvectors = framegraph.GetRes<MotionVectors>();
    if ( ! depth_buffer || ! hdr_buffer || ! ambient_buffer || ! normal_buffer || ! motionvectors )
        throw SnowEngineException( "missing resource" );

    auto& skybox = framegraph.GetRes<Skybox>();

    ForwardLightingPass::Context ctx;
    ctx.back_buffer_rtv = hdr_buffer->rtv[0];
    ctx.depth_stencil_view = depth_buffer->dsv;
    ctx.renderitems = renderitems->items;
    ctx.shadow_map_srv = shadow_maps->srv;
    ctx.pass_cb = pass_cb->pass_cb;
    ctx.ambient_rtv = ambient_buffer->rtv;
    ctx.normals_rtv = normal_buffer->rtv;
    ctx.motionvectors_rtv = motionvectors->rtv;
    ctx.shadow_cascade_srv = csm->srv;
    ctx.ibl.desc_table_srv = skybox->srv_table;
    ctx.ibl.radiance_multiplier = skybox->radiance_factor;
    ctx.ibl.transform = skybox->tf_cbv;
    ctx.use_rt_shadows = use_rt_shadows;
    if (use_rt_shadows)
        ctx.shadowmask_srv = rt_shadows->srv;
    else
        ctx.shadowmask_srv = {};

    
    PIXBeginEvent( &cmd_list, PIX_COLOR( 200, 210, 230 ), "ForwardLighting" );

    const float bgr_color[4] = { 0, 0, 0, 0 };
    cmd_list.ClearRenderTargetView( ctx.back_buffer_rtv, bgr_color, 0, nullptr );
    cmd_list.ClearRenderTargetView( ctx.ambient_rtv, bgr_color, 0, nullptr );
    cmd_list.ClearRenderTargetView( ctx.normals_rtv, bgr_color, 0, nullptr );
    cmd_list.ClearRenderTargetView( ctx.motionvectors_rtv, bgr_color, 0, nullptr );

    cmd_list.RSSetViewports( 1, &view->viewport );
    cmd_list.RSSetScissorRects( 1, &view->scissor_rect );

    m_pass.Begin( use_rt_shadows ? m_renderstates.triangle_fill_raytraced_shadows : m_renderstates.triangle_fill,
        cmd_list );

    m_pass.Draw( ctx );

    m_pass.End();

    PIXEndEvent( &cmd_list );
}
