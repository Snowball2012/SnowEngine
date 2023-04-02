#pragma once

#include "FramegraphResource.h"

#include "DepthAwareBlurPass.h"


template<class Framegraph>
class BlurSSAONode : public BaseRenderNode<Framegraph>
{
public:
    using OpenRes = std::tuple
        <
        >;
    using WriteRes = std::tuple
        <
        ResourceInState<SSAOTexture_Blurred, D3D12_RESOURCE_STATE_UNORDERED_ACCESS>,
        SSAOTexture_Transposed
        >;
    using ReadRes = std::tuple
        <
        ResourceInState<SSAOBuffer_Noisy, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE>,
        ResourceInState<DepthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_READ>,
        ForwardPassCB
        >;
    using CloseRes = std::tuple
        <
        >;

    BlurSSAONode( ID3D12Device& device )
        : m_pass( device )
    {
        m_state = m_pass.BuildRenderState( device );
    }

    virtual void Run( Framegraph& framegraph, IGraphicsCommandList& cmd_list ) override;

private:
    DepthAwareBlurPass m_pass;
    DepthAwareBlurPass::RenderStateID m_state;
};


template<class Framegraph>
inline void BlurSSAONode<Framegraph>::Run( Framegraph& framegraph, IGraphicsCommandList& cmd_list )
{
    OPTICK_EVENT();
    auto& blurred_ssao = framegraph.GetRes<SSAOTexture_Blurred>();
    auto& transposed_ssao = framegraph.GetRes<SSAOTexture_Transposed>();
    auto& noisy_ssao = framegraph.GetRes<SSAOBuffer_Noisy>();
    auto& depth_buffer = framegraph.GetRes<DepthStencilBuffer>();
    auto& pass_cb = framegraph.GetRes<ForwardPassCB>();

    if ( ! blurred_ssao || ! transposed_ssao || ! noisy_ssao
         || ! depth_buffer || ! pass_cb )
        throw SnowEngineException( "missing resource" );

    const auto& transposed_desc = transposed_ssao->res->GetDesc();
    const auto& blurred_desc = blurred_ssao->res->GetDesc();
    
    PIXBeginEvent( &cmd_list, PIX_COLOR( 200, 210, 230 ), "HBAO Blur" );

    m_pass.Begin( m_state, cmd_list );

    DepthAwareBlurPass::Context ctx;
    ctx.depth_srv = depth_buffer->srv;
    ctx.input_srv = noisy_ssao->srv;
    ctx.blurred_uav = transposed_ssao->uav;
    ctx.pass_cb = pass_cb->pass_cb;


    ctx.uav_width = transposed_desc.Width;
    ctx.uav_height = transposed_desc.Height;
    ctx.transpose_flag = false;

    m_pass.Draw( ctx );

    // TODO: let the framegraph handle this barrier
    CD3DX12_RESOURCE_BARRIER barriers[] =
    {
        CD3DX12_RESOURCE_BARRIER::Transition( transposed_ssao->res,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE )
    };

    cmd_list.ResourceBarrier( 1, barriers );

    ctx.input_srv = transposed_ssao->srv;
    ctx.blurred_uav = blurred_ssao->uav;
    ctx.transpose_flag = true;

    ctx.uav_width = blurred_desc.Width;
    ctx.uav_height = blurred_desc.Height;

    m_pass.Draw( ctx );

    m_pass.End();

    PIXEndEvent( &cmd_list );
}
