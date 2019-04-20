// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "HBAOPass.h"


HBAOPass::HBAOPass( ID3D12Device& device )
{
    m_root_signature = BuildRootSignature( device );
}


HBAOPass::RenderStateID HBAOPass::BuildRenderState( DXGI_FORMAT rtv_format, ID3D12Device& device )
{
    ComPtr<ID3D12PipelineState> pso;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc;
    ZeroMemory( &pso_desc, sizeof( D3D12_GRAPHICS_PIPELINE_STATE_DESC ) );
    pso_desc.InputLayout.NumElements = 0;
    pso_desc.pRootSignature = m_root_signature.Get();

    const auto shaders = LoadAndCompileShaders();
    pso_desc.VS =
    {
        reinterpret_cast<BYTE*>( shaders.first->GetBufferPointer() ),
        shaders.first->GetBufferSize()
    };
    pso_desc.PS =
    {
        reinterpret_cast<BYTE*>( shaders.second->GetBufferPointer() ),
        shaders.second->GetBufferSize()
    };
    pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC( D3D12_DEFAULT );
    pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso_desc.BlendState = CD3DX12_BLEND_DESC( D3D12_DEFAULT );
    pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC( D3D12_DEFAULT );
    pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pso_desc.DepthStencilState.DepthEnable = false;
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = rtv_format;
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.SampleDesc.Count = 1;
    pso_desc.SampleDesc.Quality = 0;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    ThrowIfFailedH( device.CreateGraphicsPipelineState( &pso_desc, IID_PPV_ARGS( &pso ) ) );

    return m_pso_cache.emplace( std::move( pso ) );
}


void HBAOPass::Draw( const Context& context )
{
    m_cmd_list->OMSetRenderTargets( 1, &context.ssao_rtv, false, nullptr );

    m_cmd_list->SetGraphicsRootConstantBufferView( 0, context.pass_cb );
    m_cmd_list->SetGraphicsRootDescriptorTable( 1, context.depth_srv );
    m_cmd_list->SetGraphicsRootDescriptorTable( 2, context.normals_srv );
    m_cmd_list->SetGraphicsRoot32BitConstants( 3, 5, &context.settings, 0 );

    m_cmd_list->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
    m_cmd_list->IASetIndexBuffer( nullptr );
    m_cmd_list->IASetVertexBuffers( 0, 0, nullptr );

    m_cmd_list->DrawInstanced( 3, 1, 0, 0 );
}


ComPtr<ID3D12RootSignature> HBAOPass::BuildRootSignature( ID3D12Device& device )
{
    /*
    HBAO pass root sig
    0 - pass constants
    1 - z-buffer (depth in projected space)
    2 - view space normals
    3 - settings

    Shader register bindings
    b0 - constants
    b1 - settings
    t0 - depth
    t1 - normals
    */

    constexpr int nparams = 4;

    CD3DX12_ROOT_PARAMETER slot_root_parameter[nparams];

    slot_root_parameter[0].InitAsConstantBufferView( 0, 0, D3D12_SHADER_VISIBILITY_PIXEL );

    CD3DX12_DESCRIPTOR_RANGE desc_table[2];
    desc_table[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0 );
    desc_table[1].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1 );
    slot_root_parameter[1].InitAsDescriptorTable( 1, desc_table );
    slot_root_parameter[2].InitAsDescriptorTable( 1, desc_table + 1 );
    slot_root_parameter[3].InitAsConstants( 5, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL );

    CD3DX12_STATIC_SAMPLER_DESC samplers[2] = {
        CD3DX12_STATIC_SAMPLER_DESC( // linear
            0, // shaderRegister
            D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
            D3D12_TEXTURE_ADDRESS_MODE_WRAP ), // addressW
        CD3DX12_STATIC_SAMPLER_DESC( // point
            1, // shaderRegister
            D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
            D3D12_TEXTURE_ADDRESS_MODE_WRAP ) // addressW
    };

    CD3DX12_ROOT_SIGNATURE_DESC root_sig_desc( nparams, slot_root_parameter, 2, samplers );

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


std::pair<ComPtr<ID3DBlob>, ComPtr<ID3DBlob>> HBAOPass::LoadAndCompileShaders()
{
    return std::make_pair( Utils::LoadBinary( L"shaders/fullscreen_quad_vs.cso" ), Utils::LoadBinary( L"shaders/postprocess/hbao_generation.cso" ) );
}


void HBAOPass::BeginDerived( RenderStateID state ) noexcept
{
    m_cmd_list->SetGraphicsRootSignature( m_root_signature.Get() );
}
