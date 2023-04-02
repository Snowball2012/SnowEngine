#pragma once

// simple class that stores settings and state for Temporal Anti-Aliasing

#include "FramegraphResource.h"

#include "resources/GPUDevice.h"

class TemporalBlendPass : public RenderPass
{
public:
    TemporalBlendPass(GPUDevice& device);
    
    RenderStateID BuildRenderState();
    
    struct ShaderData
    {
        float prev_frame_blend_val;
        float unjitter[2];
        float color_window_size;
    };

    struct Context
    {
        D3D12_GPU_DESCRIPTOR_HANDLE prev_frame_srv;
        D3D12_GPU_DESCRIPTOR_HANDLE jittered_frame_srv;
        D3D12_GPU_DESCRIPTOR_HANDLE cur_frame_uav;
        D3D12_GPU_DESCRIPTOR_HANDLE motion_vectors_srv;
        uint32_t frame_size[2];
        ShaderData gpu_data;
    };

    void Draw( const Context& context );

private:
    
    static ComPtr<ID3D12RootSignature> BuildRootSignature( ID3D12Device& device );
    
    static ComPtr<ID3DBlob> LoadAndCompileShaders();
    
    virtual void BeginDerived( RenderStateID state ) noexcept override;

    GPUDevice* m_device;

    ComPtr<ID3D12RootSignature> m_root_signature = nullptr;
};

class TemporalAA
{
public:
    TemporalAA() {};

    void FillShaderData( TemporalBlendPass::ShaderData& data ) const;
    void SetFrame( uint64_t frame_idx );
    uint64_t GetFrame() const { return m_frame_idx; }
    DirectX::XMMATRIX JitterProjection( const DirectX::XMMATRIX& proj, float width, float height ) const;

    bool IsJitterEnabled() const { return m_jitter_proj; }
    bool IsBlendEnabled() const { return m_blend_prev_frame; }
    
    bool& SetJitter() { return m_jitter_proj; }
    float& SetJitterVal() { return m_jitter_val; }
    bool& SetBlend() { return m_blend_prev_frame; }
    float& SetBlendFeedback() { return m_blend_feedback; }
    float& SetColorWindowExpansion() { return m_color_window_size; }

private:
    bool m_jitter_proj = true;
    float m_jitter_val = 1.0f;
    float m_cur_jitter[2] = { 0 };

    float m_color_window_size = 0.2f;

    bool m_blend_prev_frame = true;
    float m_blend_feedback = 0.9f;

    uint64_t m_frame_idx;
};

template<typename Framegraph>
class TemporalAANode : public BaseRenderNode<Framegraph>
{
public:
    using OpenRes = std::tuple
        <
        >;
    using WriteRes = std::tuple
        <
        ResourceInState<HDRBuffer_Final, D3D12_RESOURCE_STATE_UNORDERED_ACCESS>
        >;
    using ReadRes = std::tuple
        <
        ResourceInState<HDRBuffer_Prev, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE>,
        ResourceInState<HDRBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE>,
        ResourceInState<MotionVectors, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE>
        >;
    using CloseRes = std::tuple
        <
        >;

    TemporalAANode(GPUDevice& device)
        : m_pass(device)
    {
        m_state = m_pass.BuildRenderState();
    }

    void SetTAAState(TemporalAA* state)
    {
        m_taa_state = state;
    }

    virtual void Run(Framegraph& framegraph, IGraphicsCommandList& cmd_list) override
    {
        OPTICK_EVENT();
        auto& hdr_buffer = framegraph.GetRes<HDRBuffer>();
        auto& prev_hdr_buffer = framegraph.GetRes<HDRBuffer_Prev>();
        auto& new_hdr_buffer = framegraph.GetRes<HDRBuffer_Final>();
        auto& motion_vectors = framegraph.GetRes<MotionVectors>();

        if ( ! m_taa_state || ! hdr_buffer || ! prev_hdr_buffer || ! motion_vectors )
            throw SnowEngineException( "missing resource" );
        
        PIXBeginEvent( &cmd_list, PIX_COLOR( 200, 210, 230 ), "TAA" );
        m_pass.Begin( m_state, cmd_list );
        
        TemporalBlendPass::Context ctx;
        ctx.prev_frame_srv = prev_hdr_buffer->srv;
        ctx.jittered_frame_srv = hdr_buffer->srv[0];
        ctx.cur_frame_uav = new_hdr_buffer->uav;
        ctx.motion_vectors_srv = motion_vectors->srv;
        ctx.frame_size[0] = uint32_t( hdr_buffer->size.x );
        ctx.frame_size[1] = uint32_t( hdr_buffer->size.y );
        m_taa_state->FillShaderData(ctx.gpu_data);
        m_pass.Draw(ctx);
        m_pass.End();
    }

private:
    TemporalAA* m_taa_state = nullptr;
    TemporalBlendPass m_pass;
    TemporalBlendPass::RenderStateID m_state;
};
