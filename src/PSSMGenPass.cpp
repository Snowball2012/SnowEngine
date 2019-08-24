// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "stdafx.h"

#include "PSSMGenPass.h"

#include "ForwardLightingPass.h"


PSSMGenPass::PSSMGenPass( ID3D12Device& device )
{
    m_root_signature = BuildRootSignature( device );
}


PSSMGenPass::RenderStateID PSSMGenPass::BuildRenderState( DXGI_FORMAT dsv_format, int bias, bool back_culling, ID3D12Device& device )
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};

    const ::InputLayout input_layout = ForwardLightingPass::InputLayout();
    const auto shaders = LoadAndCompileShaders();

    pso_desc.InputLayout = { input_layout.data(), UINT( input_layout.size() ) };
    pso_desc.pRootSignature = m_root_signature.Get();

    pso_desc.VS =
    {
        reinterpret_cast<BYTE*>( shaders.vs->GetBufferPointer() ),
        shaders.vs->GetBufferSize()
    };

    pso_desc.GS =
    {
        reinterpret_cast<BYTE*>( shaders.gs->GetBufferPointer() ),
        shaders.gs->GetBufferSize()
    };

    pso_desc.PS =
    {
        reinterpret_cast<BYTE*>( shaders.ps->GetBufferPointer() ),
        shaders.ps->GetBufferSize()
    };

    pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC( D3D12_DEFAULT );
    pso_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;
    if ( bias > 0 )
    {
        pso_desc.RasterizerState.DepthBias = bias;
        pso_desc.RasterizerState.SlopeScaledDepthBias = 1.0f;
    }
    pso_desc.RasterizerState.CullMode = back_culling ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_FRONT;
    pso_desc.BlendState = CD3DX12_BLEND_DESC( D3D12_DEFAULT );
    pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC( D3D12_DEFAULT );

    pso_desc.SampleMask = UINT_MAX;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    pso_desc.NumRenderTargets = 0;
    pso_desc.SampleDesc.Count = 1;
    pso_desc.SampleDesc.Quality = 0;

    pso_desc.DSVFormat = dsv_format;

    RenderStateID state = m_pso_cache.emplace();

    ThrowIfFailedH( device.CreateGraphicsPipelineState( &pso_desc, IID_PPV_ARGS( &m_pso_cache[state] ) ) );

    return state;
}


void PSSMGenPass::Draw( const Context& context )
{
    m_cmd_list->OMSetRenderTargets( 0, nullptr, false, &context.depth_stencil_view );

    m_cmd_list->SetGraphicsRoot32BitConstant( 2, context.light_idx, 0 );
    m_cmd_list->SetGraphicsRootConstantBufferView( 3, context.pass_cbv );

    for ( const auto& item_span : context.renderitems )
    {
        for ( const auto& render_item : item_span )
        {
            if ( ! render_item.material->BindDataToPipeline( FramegraphTechnique::ShadowGenPass, render_item.item_id, *m_cmd_list ) )
                throw SnowEngineException( "couldn't bind a material to the pipeline" );

            m_cmd_list->SetGraphicsRootConstantBufferView( 0, render_item.per_object_cb );            

            m_cmd_list->IASetVertexBuffers( 0, 1, &render_item.vbv );
            m_cmd_list->IASetIndexBuffer( &render_item.ibv );
            m_cmd_list->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
            m_cmd_list->DrawIndexedInstanced( render_item.index_count, render_item.instance_count, render_item.index_offset, render_item.vertex_offset, 0 );
        }
    }
}


ComPtr<ID3D12RootSignature> PSSMGenPass::BuildRootSignature( ID3D12Device& device )
{
    /*
        pssm generation pass root sig
         0 - object cbv
         1 - base color map
         2 - light index
         3 - pass cbv

         Shader register bindings
         b0 - object cbv
         b1 - light index
         b2 - pass cbv

         t0 - base color
         s0 - sampler
    */

    constexpr int nparams = 4;

    CD3DX12_ROOT_PARAMETER1 slot_root_parameter[nparams];

    slot_root_parameter[0].InitAsConstantBufferView( 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_VERTEX );

    CD3DX12_DESCRIPTOR_RANGE1 desc_table;
    desc_table.Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC );
    slot_root_parameter[1].InitAsDescriptorTable( 1, &desc_table, D3D12_SHADER_VISIBILITY_PIXEL );

    slot_root_parameter[2].InitAsConstants( 1, 1, D3D12_SHADER_VISIBILITY_GEOMETRY );
    slot_root_parameter[3].InitAsConstantBufferView( 2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC );

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        0, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
        0.0f,                             // mipLODBias
        8 );                               // maxAnisotropy

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_sig_desc( nparams, slot_root_parameter,
                                               1, &anisotropicWrap,
                                               D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT );

    ComPtr<ID3DBlob> serialized_root_sig = nullptr;
    ComPtr<ID3DBlob> error_blob = nullptr;
    HRESULT hr = D3DX12SerializeVersionedRootSignature( &root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1_1,
                                              serialized_root_sig.GetAddressOf(), error_blob.GetAddressOf() );

    if ( error_blob )
        OutputDebugStringA( (char*)error_blob->GetBufferPointer() );
    ThrowIfFailedH( hr );

    ComPtr<ID3D12RootSignature> rootsig;

    ThrowIfFailedH( device.CreateRootSignature(
        0,
        serialized_root_sig->GetBufferPointer(),
        serialized_root_sig->GetBufferSize(),
        IID_PPV_ARGS( &rootsig ) ) );

    return rootsig;
}


PSSMGenPass::Shaders PSSMGenPass::LoadAndCompileShaders()
{
    return Shaders{ Utils::LoadBinary( L"shaders/pssm_vs.cso" ), Utils::LoadBinary( L"shaders/pssm_gs.cso" ), Utils::LoadBinary( L"shaders/depth_prepass_ps.cso" ) };
}


void PSSMGenPass::BeginDerived( RenderStateID state ) noexcept
{
    m_cmd_list->SetGraphicsRootSignature( m_root_signature.Get() );
}
