#include "StdAfx.h"

#include "ShaderPrograms.h"

#include "Rendergraph.h"

BlitTextureProgram::BlitTextureProgram()
    : ShaderProgram()
{
    m_type = ShaderProgramType::Raster;

    {
        RHI::DescriptorViewRange ranges[2] = {};

        ranges[0].type = RHIShaderBindingType::TextureRO;
        ranges[0].count = 1;
        ranges[0].stages = RHIShaderStageFlags::PixelShader;

        ranges[1].type = RHIShaderBindingType::Sampler;
        ranges[1].count = 1;
        ranges[1].stages = RHIShaderStageFlags::PixelShader;

        RHI::DescriptorSetLayoutInfo binding_table = {};
        binding_table.ranges = ranges;
        binding_table.range_count = std::size( ranges );

        m_dsl = GetRHI().CreateDescriptorSetLayout( binding_table );

        RHI::ShaderBindingLayoutInfo layout_info = {};
        RHIDescriptorSetLayout* dsls[1] = { m_dsl.get() };
        layout_info.tables = dsls;
        layout_info.table_count = std::size( dsls );

        m_layout = GetRHI().CreateShaderBindingLayout( layout_info );
    }

    {
        RHI::ShaderCreateInfo create_info = {};
        std::wstring shader_path = ToWString( ToOSPath( "#engine/shaders/FullscreenQuad.hlsl" ).c_str() );
        create_info.filename = shader_path.c_str();

        create_info.frequency = RHI::ShaderFrequency::Vertex;
        create_info.entry_point = "FullScreenQuadVS";
        m_vs = GetRHI().CreateShader( create_info );

        create_info.frequency = RHI::ShaderFrequency::Pixel;
        create_info.entry_point = "FullScreenQuadPS";
        m_ps = GetRHI().CreateShader( create_info );
    }
}

const RHIGraphicsPipeline* BlitTextureProgram::GetPSO( RHIFormat output_format ) const
{
    auto& pso = m_psos[output_format];

    if ( pso == nullptr )
    {
        RHIInputAssemblerInfo ia_info = {};

        RHIGraphicsPipelineInfo rhi_pipeline_info = {};

        rhi_pipeline_info.input_assembler = &ia_info;
        rhi_pipeline_info.vs = m_vs.get();
        rhi_pipeline_info.ps = m_ps.get();

        RHIPipelineRTInfo rt_info = {};
        rt_info.format = output_format;
        rhi_pipeline_info.rts_count = 1;
        rhi_pipeline_info.rt_info = &rt_info;
        rhi_pipeline_info.binding_layout = m_layout.get();

        rhi_pipeline_info.rasterizer.cull_mode = RHICullModeFlags::None;

        pso = GetRHI().CreatePSO( rhi_pipeline_info );
    }

    return pso.get();
}

bool BlitTextureProgram::Run( RHICommandList& cmd_list, Rendergraph& rg, const Params& parms ) const
{
    const RHIGraphicsPipeline* pso = GetPSO( parms.output_format );

    if ( !SE_ENSURE( pso ) )
        return false;

    RHIPassRTVInfo rt = {};
    rt.load_op = RHILoadOp::Clear;
    rt.store_op = RHIStoreOp::Store;
    rt.rtv = parms.output;
    rt.clear_value.float32[3] = 1.0f;

    RHIPassInfo pass_info = {};
    pass_info.render_area = RHIRect2D{ .offset = glm::ivec2( 0,0 ), .extent = parms.extent };
    pass_info.render_targets = &rt;
    pass_info.render_targets_count = 1;
    cmd_list.BeginPass( pass_info );

    cmd_list.SetPSO( *pso );

    RHIDescriptorSet* pass_descset = rg.AllocateFrameDescSet( *m_dsl );

    RHIViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    glm::uvec2 swapchain_extent = parms.extent;
    viewport.width = float( swapchain_extent.x );
    viewport.height = float( swapchain_extent.y );
    viewport.min_depth = 0.0f;
    viewport.max_depth = 1.0f;

    RHIRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = swapchain_extent;

    cmd_list.SetViewports( 0, &viewport, 1 );
    cmd_list.SetScissors( 0, &scissor, 1 );

    pass_descset->BindTextureROView( 0, 0, *parms.input );
    pass_descset->BindSampler( 1, 0, *parms.sampler );
    cmd_list.BindDescriptorSet( 0, *pass_descset );

    cmd_list.Draw( 3, 1, 0, 0 );

    cmd_list.EndPass();

    return true;
}
