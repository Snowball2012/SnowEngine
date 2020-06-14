// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"
#include "ToneMappingPass.h"

ToneMappingPass::ToneMappingPass( ID3D12Device& device )
{
    m_root_signature = BuildRootSignature( device );
}


ToneMappingPass::RenderStateID ToneMappingPass::BuildRenderState( ID3D12Device& device )
{
    ComPtr<ID3D12PipelineState> pso;

    D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc;
    ZeroMemory( &pso_desc, sizeof( D3D12_COMPUTE_PIPELINE_STATE_DESC ) );
    pso_desc.pRootSignature = m_root_signature.Get();

    const auto shader = LoadAndCompileShaders();
    pso_desc.CS =
    {
        reinterpret_cast<BYTE*>( shader->GetBufferPointer() ),
        shader->GetBufferSize()
    };

    ThrowIfFailedH( device.CreateComputePipelineState( &pso_desc, IID_PPV_ARGS( &pso ) ) );

    return m_pso_cache.emplace( std::move( pso ) );
}

void ToneMappingPass::Draw( const Context& context )
{
    assert( m_cmd_list );

    m_cmd_list->SetComputeRoot32BitConstants( 0, 5, &context.gpu_data, 0 );
    m_cmd_list->SetComputeRootDescriptorTable( 1, context.hdr_srv );
    m_cmd_list->SetComputeRootDescriptorTable( 2, context.sdr_uav );

    constexpr uint32_t GroupSizeX = 8;
    constexpr uint32_t GroupSizeY = 8;

    m_cmd_list->Dispatch(
        ( context.frame_size[0] + GroupSizeX - 1 ) / GroupSizeX,
        ( context.frame_size[1] + GroupSizeY - 1 ) / GroupSizeY,
        1 );
}

ComPtr<ID3D12RootSignature> ToneMappingPass::BuildRootSignature( ID3D12Device& device )
{
    /*
    Tone mapping pass root sig
    0 - constants( min luminance, max luminance, enable blending )
    1 - hdr texture srv
    2 - sdr uav

    Shader register bindings
    b0 - constants
    t0 - frame
    u0 - uav
    */

    constexpr int nparams = 3;

    CD3DX12_ROOT_PARAMETER slot_root_parameter[nparams];

    slot_root_parameter[0].InitAsConstants( 5, 0 );
    CD3DX12_DESCRIPTOR_RANGE desc_table[2];
    desc_table[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0 );
    slot_root_parameter[1].InitAsDescriptorTable( 1, desc_table );
    desc_table[1].Init( D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0 );
    slot_root_parameter[2].InitAsDescriptorTable( 1, desc_table + 1 );

    CD3DX12_STATIC_SAMPLER_DESC linear_clamp_sampler( 0, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP );
    CD3DX12_ROOT_SIGNATURE_DESC root_sig_desc( nparams, slot_root_parameter,
                                               1, &linear_clamp_sampler );

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

    rootsig->SetName( L"Tonemapping Rootsig" );

    return rootsig;
}


ComPtr<ID3DBlob> ToneMappingPass::LoadAndCompileShaders()
{
    return Utils::LoadBinary( L"shaders/tonemap.cso" );
}

void ToneMappingPass::BeginDerived( RenderStateID state ) noexcept
{
    m_cmd_list->SetComputeRootSignature( m_root_signature.Get() );
}
