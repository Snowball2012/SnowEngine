// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "TemporalAA.h"

#include "Shader.h"

using namespace DirectX;

void TemporalAA::FillShaderData( TemporalBlendPass::ShaderData& data ) const
{
    data.color_window_size = m_color_window_size;
    data.prev_frame_blend_val = m_blend_feedback;
    if ( IsJitterEnabled() )
    {
        data.unjitter[0] = m_cur_jitter[0];
        data.unjitter[1] = -m_cur_jitter[1];
    }
    else
    {
        data.unjitter[0] = 0;
        data.unjitter[1] = 0;
    }
}

void TemporalAA::SetFrame( uint64_t frame_idx )
{
    int sample = frame_idx % 8;

    m_cur_jitter[0] = HaltonSequence23[sample][0] - 0.5f;
    m_cur_jitter[1] = HaltonSequence23[sample][1] - 0.5f;
    m_frame_idx = frame_idx;
}

DirectX::XMMATRIX TemporalAA::JitterProjection( const DirectX::XMMATRIX& proj, float width, float height ) const
{
    XMMATRIX proj_jittered = proj;
    if ( m_jitter_proj )
    {
        const float sample_size_x = m_jitter_val / width;
        const float sample_size_y = m_jitter_val / height;

        proj_jittered.r[2].m128_f32[0] += m_cur_jitter[0] * sample_size_x;
        proj_jittered.r[2].m128_f32[1] += m_cur_jitter[1] * sample_size_y;
    }
    return proj_jittered;
}

TemporalBlendPass::TemporalBlendPass(GPUDevice& device)
    : m_device(&device)
{
    m_root_signature = BuildRootSignature(*m_device->GetNativeDevice());
}

RenderPass::RenderStateID TemporalBlendPass::BuildRenderState()
{
    ComPtr<ID3D12PipelineState> pso;
    
    D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc;
    ZeroMemory( &pso_desc, sizeof( D3D12_COMPUTE_PIPELINE_STATE_DESC ) );
    pso_desc.pRootSignature = m_root_signature.Get();

    auto shader = LoadAndCompileShaders();

    pso_desc.CS =
    {
        reinterpret_cast<BYTE*>( shader->GetBufferPointer() ),
        shader->GetBufferSize()
    };

    ThrowIfFailedH( m_device->GetNativeDevice()->CreateComputePipelineState( &pso_desc, IID_PPV_ARGS( &pso ) ) );

    return m_pso_cache.emplace(std::move(pso));
}

void TemporalBlendPass::BeginDerived(RenderStateID state) noexcept
{
    m_cmd_list->SetComputeRootSignature( m_root_signature.Get() );
}

void TemporalBlendPass::Draw( const Context& context )
{
    assert( m_cmd_list );

    m_cmd_list->SetComputeRoot32BitConstants( 0, 4, &context.gpu_data, 0 );
    m_cmd_list->SetComputeRootDescriptorTable( 1, context.prev_frame_srv );
    m_cmd_list->SetComputeRootDescriptorTable( 2, context.jittered_frame_srv );
    m_cmd_list->SetComputeRootDescriptorTable( 3, context.cur_frame_uav );
    m_cmd_list->SetComputeRootDescriptorTable( 4, context.motion_vectors_srv );

    constexpr uint32_t GroupSizeX = 8;
    constexpr uint32_t GroupSizeY = 8;

    m_cmd_list->Dispatch(
        ( context.frame_size[0] + GroupSizeX - 1 ) / GroupSizeX,
        ( context.frame_size[1] + GroupSizeY - 1 ) / GroupSizeY,
        1 );
}

ComPtr<ID3D12RootSignature> TemporalBlendPass::BuildRootSignature( ID3D12Device& device )
{
    /*
    Temporal blending pass root sig
    0 - constants( blend factor, unjitter vector, color_window_size )
    1 - previous frame texture
    2 - current jittered frame texture
    3 - anti-aliased output
    4 - motion vectors

    Shader register bindings
    b0 - blend factor
    t0 - prev frame
    */

    constexpr int nparams = 5;

    CD3DX12_ROOT_PARAMETER slot_root_parameter[nparams];

    slot_root_parameter[0].InitAsConstants( 4, 0 );
    CD3DX12_DESCRIPTOR_RANGE desc_table[4];
    desc_table[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0 );
    desc_table[1].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1 );
    desc_table[2].Init( D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0 );
    desc_table[3].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2 );
    slot_root_parameter[1].InitAsDescriptorTable( 1, desc_table );
    slot_root_parameter[2].InitAsDescriptorTable( 1, desc_table + 1 );
    slot_root_parameter[3].InitAsDescriptorTable( 1, desc_table + 2 );
    slot_root_parameter[4].InitAsDescriptorTable( 1, desc_table + 3 );

    CD3DX12_STATIC_SAMPLER_DESC linear_wrap(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP ); // addressW

    CD3DX12_ROOT_SIGNATURE_DESC root_sig_desc( nparams, slot_root_parameter,
                                               1, &linear_wrap );

    ComPtr<ID3DBlob> serialized_root_sig = nullptr;
    ComPtr<ID3DBlob> error_blob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature( &root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                              serialized_root_sig.GetAddressOf(), error_blob.GetAddressOf() );

    if ( error_blob )
    {
        OutputDebugStringA( (char*)error_blob->GetBufferPointer() );
    }
    ThrowIfFailedH( hr );

    ComPtr<ID3D12RootSignature> rootsig;

    ThrowIfFailedH( device.CreateRootSignature(
        0,
        serialized_root_sig->GetBufferPointer(),
        serialized_root_sig->GetBufferSize(),
        IID_PPV_ARGS( &rootsig ) ) );

    return rootsig;
}

ComPtr<ID3DBlob> TemporalBlendPass::LoadAndCompileShaders()
{
    ComPtr<ID3DBlob> bytecode;
    Shader taa_shader(L"shaders/taa.hlsl", ShaderFrequency::Compute, L"main", {}, nullptr);
    ThrowIfFailedH(taa_shader.GetNativeBytecode()->QueryInterface(IID_PPV_ARGS(bytecode.GetAddressOf())));
    return bytecode;
}
