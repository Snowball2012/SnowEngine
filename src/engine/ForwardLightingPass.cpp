// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"
#include "ForwardLightingPass.h"

#include "RenderUtils.h"
#include "Shader.h"

ForwardLightingPass::ForwardLightingPass( ID3D12Device& device )
{
    m_root_signature = BuildRootSignature( Utils::StaticSamplers(), device );
}


ForwardLightingPass::States ForwardLightingPass::CompileStates( DXGI_FORMAT rendertarget_format,
                                                                DXGI_FORMAT ambient_rtv_format,
                                                                DXGI_FORMAT normal_format,
                                                                DXGI_FORMAT depth_stencil_format,
                                                                DXGI_FORMAT motion_vectors_format,
                                                                ID3D12Device& device )
{
    const ::InputLayout input_layout = InputLayout();

    const Shaders shaders = LoadAndCompileShaders();

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc;
    ZeroMemory( &pso_desc, sizeof( D3D12_GRAPHICS_PIPELINE_STATE_DESC ) );

    pso_desc.InputLayout = { input_layout.data(), UINT( input_layout.size() ) };
    pso_desc.pRootSignature = m_root_signature.Get();

    pso_desc.VS =
    {
        reinterpret_cast<BYTE*>( shaders.vs->GetBufferPointer() ),
        shaders.vs->GetBufferSize()
    };

    pso_desc.PS =
    {
        reinterpret_cast<BYTE*>( shaders.ps->GetBufferPointer() ),
        shaders.ps->GetBufferSize()
    };

    pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC( D3D12_DEFAULT );
    pso_desc.BlendState = CD3DX12_BLEND_DESC( D3D12_DEFAULT );
    pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC( D3D12_DEFAULT );
    pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
    pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    pso_desc.NumRenderTargets = 4;
    pso_desc.RTVFormats[0] = rendertarget_format;
    pso_desc.RTVFormats[1] = ambient_rtv_format;
    pso_desc.RTVFormats[2] = normal_format;
    pso_desc.RTVFormats[3] = motion_vectors_format;
    
    pso_desc.SampleDesc.Count = 1;
    pso_desc.SampleDesc.Quality = 0;

    pso_desc.DSVFormat = depth_stencil_format;

    ComPtr<ID3D12PipelineState> main_pso, wireframe_pso, rt_pso;
    ThrowIfFailedH( device.CreateGraphicsPipelineState( &pso_desc, IID_PPV_ARGS( &main_pso ) ) );

    pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    ThrowIfFailedH( device.CreateGraphicsPipelineState( &pso_desc, IID_PPV_ARGS( &wireframe_pso ) ) );

    pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC( D3D12_DEFAULT );
    pso_desc.PS =
    {
        reinterpret_cast<BYTE*>( shaders.ps_raytraced->GetBufferPointer() ),
        shaders.ps_raytraced->GetBufferSize()
    };
    ThrowIfFailedH( device.CreateGraphicsPipelineState( &pso_desc, IID_PPV_ARGS( &rt_pso ) ) );

    States retval;
    retval.triangle_fill = m_pso_cache.emplace( std::move( main_pso ) );
    retval.wireframe = m_pso_cache.emplace( std::move( wireframe_pso ) );
    retval.triangle_fill_raytraced_shadows = m_pso_cache.emplace( std::move( rt_pso ) );

    return retval;
}


void ForwardLightingPass::Draw( const Context& context )
{	
    D3D12_CPU_DESCRIPTOR_HANDLE render_targets[4] =
    {
        context.back_buffer_rtv,
        context.ambient_rtv,
        context.normals_rtv,
        context.motionvectors_rtv
    };

    m_cmd_list->OMSetRenderTargets( 4, render_targets, false, &context.depth_stencil_view );

    m_cmd_list->SetGraphicsRootDescriptorTable( 3, context.use_rt_shadows ? context.shadowmask_srv : context.shadow_map_srv );
    m_cmd_list->SetGraphicsRootDescriptorTable( 4, context.shadow_cascade_srv );

    m_cmd_list->SetGraphicsRootConstantBufferView( 5, context.pass_cb );
    m_cmd_list->SetGraphicsRootDescriptorTable( 6, context.ibl.desc_table_srv );
    m_cmd_list->SetGraphicsRootConstantBufferView( 7, context.ibl.transform );
    m_cmd_list->SetGraphicsRoot32BitConstants( 8, 1, &context.ibl.radiance_multiplier, 0 );

    m_cmd_list->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    for ( const auto& item_span : context.renderitems )
    {
        for ( const auto& render_item : item_span )
        {
            if ( ! render_item.material->BindDataToPipeline( FramegraphTechnique::ForwardZPass, render_item.item_id, *m_cmd_list ) )
                throw SnowEngineException( "couldn't bind a material to the pipeline" );

            m_cmd_list->SetGraphicsRootConstantBufferView( 0, render_item.per_object_cb );            

            m_cmd_list->IASetVertexBuffers( 0, 1, &render_item.vbv );
            m_cmd_list->IASetIndexBuffer( &render_item.ibv );
            m_cmd_list->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
            m_cmd_list->DrawIndexedInstanced( render_item.index_count, render_item.instance_count, render_item.index_offset, render_item.vertex_offset, 0 );
        }
    }
}


ForwardLightingPass::Shaders ForwardLightingPass::LoadAndCompileShaders()
{
    Shaders base_shaders = {};
    
    ShaderCompiler* compiler = ShaderCompiler::Get();
    if (!SE_ENSURE(compiler))
        return base_shaders;

    std::unique_ptr<ShaderSourceFile> forward_source = compiler->LoadSourceFile(L"shaders/forward.hlsl");
    if (!SE_ENSURE(forward_source))
        return base_shaders;

    Shader vs(*forward_source, ShaderFrequency::Vertex, L"main_vs");
    Shader ps(*forward_source, ShaderFrequency::Pixel, L"main_ps");

    
    std::vector<ShaderDefine> defines;
    auto& rt_define = defines.emplace_back();
    rt_define.name = L"RAYTRACED_SHADOWS";
    rt_define.value = L"1";

    Shader raytraced_ps(*forward_source, ShaderFrequency::Pixel, L"main_ps", std::move(defines));
    
    ThrowIfFailedH(vs.GetNativeBytecode()->QueryInterface(IID_PPV_ARGS(base_shaders.vs.GetAddressOf())));
    ThrowIfFailedH(ps.GetNativeBytecode()->QueryInterface(IID_PPV_ARGS(base_shaders.ps.GetAddressOf())));
    ThrowIfFailedH(raytraced_ps.GetNativeBytecode()->QueryInterface(IID_PPV_ARGS(base_shaders.ps_raytraced.GetAddressOf())));
    
    return base_shaders;
}


void ForwardLightingPass::BeginDerived( RenderStateID state ) noexcept
{
    m_cmd_list->SetGraphicsRootSignature( m_root_signature.Get() );
}
