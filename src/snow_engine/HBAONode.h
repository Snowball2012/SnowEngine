#pragma once

#include "FramegraphResource.h"

#include "HBAOPass.h"


template<class Framegraph>
class HBAONode : public BaseRenderNode<Framegraph>
{
public:
    using OpenRes = std::tuple
        <
        >;
    using WriteRes = std::tuple
        <
        ResourceInState<SSAOBuffer_Noisy, D3D12_RESOURCE_STATE_RENDER_TARGET>,
        ResourceInState<HDRBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE>
        >;
    using ReadRes = std::tuple
        <
        ResourceInState<NormalBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE>,
        ResourceInState<DepthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_READ>,
        ScreenConstants,
        ForwardPassCB,
        HBAOSettings
        >;
    using CloseRes = std::tuple
        <
        >;

    HBAONode( DXGI_FORMAT rtv_format, ID3D12Device& device )
        : m_pass( device )
    {
        m_state = m_pass.BuildRenderState( rtv_format, device );
    }

    virtual void Run( Framegraph& framegraph, IGraphicsCommandList& cmd_list ) override
    {
        OPTICK_EVENT();
        auto& ssao_buffer = framegraph.GetRes<SSAOBuffer_Noisy>();
        if ( ! ssao_buffer )
            throw SnowEngineException( "missing resource" );

        auto& normal_buffer = framegraph.GetRes<NormalBuffer>();
        auto& depth_buffer = framegraph.GetRes<DepthStencilBuffer>();
        auto& forward_cb = framegraph.GetRes<ForwardPassCB>();
        auto& settings = framegraph.GetRes<HBAOSettings>();
        auto& direct_light_buffer = framegraph.GetRes<HDRBuffer>();
        if ( ! normal_buffer || ! depth_buffer
             || ! direct_light_buffer
             || ! forward_cb || ! settings )
            throw SnowEngineException( "missing resource" );
        
        PIXBeginEvent( &cmd_list, PIX_COLOR( 200, 210, 230 ), "HBAO Noisy" );
        m_pass.Begin( m_state, cmd_list );

        const auto& storage_desc = ssao_buffer->res->GetDesc();
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

        settings->data.render_target_size = DirectX::XMFLOAT2( viewport.Width, viewport.Height );

        HBAOPass::Context ctx;
        ctx.depth_srv = depth_buffer->srv;
        ctx.normals_srv = normal_buffer->srv;
        ctx.direct_lighting_srv = direct_light_buffer->srv[0];
        ctx.pass_cb = forward_cb->pass_cb;
        ctx.ssao_rtv = ssao_buffer->rtv;
        ctx.settings = settings->data;

        m_pass.Draw( ctx );

        m_pass.End();

        PIXEndEvent( &cmd_list );
    }

private:
    HBAOPass m_pass;
    HBAOPass::RenderStateID m_state;
};