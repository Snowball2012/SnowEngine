// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "SingleDownsamplerPass.h"


SingleDownsamplerPass::SingleDownsamplerPass( ID3D12Device& device )
{
    m_root_signature = BuildRootSignature( device );
}


SingleDownsamplerPass::RenderStateID SingleDownsamplerPass::BuildRenderState( ID3D12Device& device )
{
    ComPtr<ID3D12PipelineState> pso;

    D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc;
    ZeroMemory( &pso_desc, sizeof( D3D12_COMPUTE_PIPELINE_STATE_DESC ) );

    const auto shader = LoadAndCompileShader();
    pso_desc.CS =        
    {
        reinterpret_cast<BYTE*>( shader->GetBufferPointer() ),
        shader->GetBufferSize()
    };
    pso_desc.pRootSignature = m_root_signature.Get();

    ThrowIfFailedH( device.CreateComputePipelineState( &pso_desc, IID_PPV_ARGS( &pso ) ) );

    return m_pso_cache.emplace( std::move( pso ) );
}


namespace
{
    enum class RootSigSlot : int
    {
        ShaderCB = 0,
        MipsUAVs,
        AtomicCounter,
        Count
    };
}


void SingleDownsamplerPass::Draw( const Context& context )
{
    assert( m_cmd_list );
    assert( context.shader_cb_mapped.size() >= sizeof( ShaderCB ) );
    assert( context.mip_size.size() > 1 ); // otherwise why are we here?

    // each group process 64x64 block
    uint32_t ngroups_x = ( context.mip_size[0].x + 63 ) / 64;
    uint32_t ngroups_y = ( context.mip_size[0].y + 63 ) / 64;

    ShaderCB cb;
    {
        cb.nmips = uint32_t( context.mip_size.size() );
        cb.ngroups = ngroups_x * ngroups_y;
        for ( uint32_t i = 0; i < cb.nmips; ++i )
        {
            cb.mip_size[i * 4] = context.mip_size[i].x;
            cb.mip_size[i * 4 + 1] = context.mip_size[i].y;
        }
    }
    memcpy( context.shader_cb_mapped.begin(), &cb, sizeof( cb ) );

    m_cmd_list->SetComputeRootConstantBufferView( UINT( RootSigSlot::ShaderCB ), context.shader_cb_gpu );
    m_cmd_list->SetComputeRootDescriptorTable( UINT( RootSigSlot::MipsUAVs ), context.mip_uav_table );
    m_cmd_list->SetComputeRootDescriptorTable( UINT( RootSigSlot::AtomicCounter ), context.global_atomic_counter_uav );

    m_cmd_list->Dispatch( ngroups_x, ngroups_y, 1 );
}


ComPtr<ID3D12RootSignature> SingleDownsamplerPass::BuildRootSignature( ID3D12Device& device )
{
    constexpr int nparams = int( RootSigSlot::Count );

    CD3DX12_ROOT_PARAMETER slot_root_parameter[nparams];
    
    slot_root_parameter[int( RootSigSlot::ShaderCB )].InitAsConstantBufferView( 0 );

    CD3DX12_DESCRIPTOR_RANGE desc_table[2];
    desc_table[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_UAV, MaxMips, 0 );
    slot_root_parameter[int( RootSigSlot::MipsUAVs )].InitAsDescriptorTable( 1, desc_table );
    
    desc_table[1].Init( D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, MaxMips );
    slot_root_parameter[int( RootSigSlot::AtomicCounter )].InitAsDescriptorTable( 1, desc_table + 1 );

    CD3DX12_ROOT_SIGNATURE_DESC root_sig_desc( nparams, slot_root_parameter );

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
    rootsig->SetName( L"Singlepass Downsampler Rootsig" );

    return rootsig;
}


ComPtr<ID3DBlob> SingleDownsamplerPass::LoadAndCompileShader()
{
    return Utils::LoadBinary( L"shaders/downsample_singlepass.cso" );
}


void SingleDownsamplerPass::BeginDerived( RenderStateID state ) noexcept
{
    m_cmd_list->SetComputeRootSignature( m_root_signature.Get() );
}
