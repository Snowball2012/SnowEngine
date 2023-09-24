#include "../StdAfx.h"

#include "DebugDrawing.h"

#include <Engine/Rendergraph.h>
#include <Engine/Scene.h>
#include <Engine/ShaderPrograms.h>


namespace
{
    static constexpr uint32_t MAX_DEBUG_LINES = 1024;

    struct GPUDebugLine
    {
        glm::vec3 start_ws;
        glm::vec3 end_ws;
        glm::vec3 color_start;
        glm::vec3 color_end;
    };

    struct GPUDebugLineIndirectArgs
    {
        uint32_t vertex_per_instance;
        uint32_t num_instances;
        uint32_t start_vertex_location;
        uint32_t start_instance_location;
    };
}


// DebugDrawingInitProgram

class DebugDrawingInitProgram : public ShaderProgram
{
private:
    RHIDescriptorSetLayoutPtr m_dsl;

    RHIShaderPtr m_cs = nullptr;

    RHIComputePipelinePtr m_pso;

public:
    struct Params
    {
        RHIBufferViewInfo indirect_args_buf;
    };

    DebugDrawingInitProgram()
        : ShaderProgram()
    {
        m_type = ShaderProgramType::Compute;
        {
            RHI::DescriptorViewRange ranges[1] = {};

            ranges[0].type = RHIShaderBindingType::StructuredBuffer;
            ranges[0].count = 1;
            ranges[0].stages = RHIShaderStageFlags::ComputeShader;

            RHI::DescriptorSetLayoutInfo dsl_info = {};
            dsl_info.ranges = ranges;
            dsl_info.range_count = std::size( ranges );

            m_dsl = GetRHI().CreateDescriptorSetLayout( dsl_info );
            VERIFY_NOT_EQUAL( m_dsl, nullptr );

            RHI::ShaderBindingLayoutInfo layout_info = {};
            RHIDescriptorSetLayout* dsls[1] = { m_dsl.get() };
            layout_info.tables = dsls;
            layout_info.table_count = std::size( dsls );

            m_layout = GetRHI().CreateShaderBindingLayout( layout_info );
            VERIFY_NOT_EQUAL( m_layout, nullptr );
        }

        {
            RHI::ShaderCreateInfo create_info = {};
            std::wstring shader_path = ToWString( ToOSPath( "#engine/shaders/DebugDrawing.hlsl" ).c_str() );
            create_info.filename = shader_path.c_str();

            create_info.frequency = RHI::ShaderFrequency::Compute;
            create_info.entry_point = "DebugDrawingInitCS";
            m_cs = GetRHI().CreateShader( create_info );
            VERIFY_NOT_EQUAL( m_cs, nullptr );
        }

        {
            RHIComputePipelineInfo pso_info = {};
            pso_info.compute_shader = m_cs.get();
            pso_info.binding_layout = m_layout.get();

            m_pso = GetRHI().CreatePSO( pso_info );
            VERIFY_NOT_EQUAL( m_pso, nullptr );
        }
    }

    bool Run( RHICommandList& cmd_list, Rendergraph& rg, const Params& parms ) const
    {
        if ( !SE_ENSURE( parms.indirect_args_buf.buffer != nullptr ) )
            return false;

        cmd_list.SetPSO( *m_pso );

        RHIDescriptorSet* pass_descset = rg.AllocateFrameDescSet( *m_dsl );

        pass_descset->BindStructuredBuffer( 0, 0, parms.indirect_args_buf );
        cmd_list.BindDescriptorSet( 0, *pass_descset );

        cmd_list.Dispatch( glm::uvec3( 1, 1, 1 ) );

        return true;
    }
};

class DebugDrawingDrawProgram : public ShaderProgram
{
private:

    RHIShaderPtr m_vs = nullptr;
    RHIShaderPtr m_ps = nullptr;

    RHIDescriptorSetLayoutPtr m_dsl;

    mutable std::unordered_map<RHIFormat, RHIGraphicsPipelinePtr> m_psos;

public:
    struct Params
    {
        RHIRenderTargetView* output = nullptr;
        RHIDescriptorSet* view_descset = nullptr;
        RHIBufferViewInfo indirect_args = {};
    };

    DebugDrawingDrawProgram( RHIDescriptorSetLayoutPtr view_dsl )
        : ShaderProgram()
    {
        m_type = ShaderProgramType::Raster;
        {
            // this is a dummy empty dsl. @todo - let some DSLs in pipeline layout be empty
            RHI::DescriptorViewRange ranges[1] = {};

            ranges[0].type = RHIShaderBindingType::StructuredBuffer;
            ranges[0].count = 1;
            ranges[0].stages = RHIShaderStageFlags::ComputeShader;

            RHI::DescriptorSetLayoutInfo dsl_info = {};
            dsl_info.ranges = ranges;
            dsl_info.range_count = std::size( ranges );

            m_dsl = GetRHI().CreateDescriptorSetLayout( dsl_info );
            VERIFY_NOT_EQUAL( m_dsl, nullptr );

            RHI::ShaderBindingLayoutInfo layout_info = {};
            RHIDescriptorSetLayout* dsls[2] = { m_dsl.get(), view_dsl.get()};
            layout_info.tables = dsls;
            layout_info.table_count = std::size( dsls );

            m_layout = GetRHI().CreateShaderBindingLayout( layout_info );
            VERIFY_NOT_EQUAL( m_layout, nullptr );
        }

        {
            RHI::ShaderCreateInfo create_info = {};
            std::wstring shader_path = ToWString( ToOSPath( "#engine/shaders/DebugDrawing.hlsl" ).c_str() );
            create_info.filename = shader_path.c_str();

            create_info.frequency = RHI::ShaderFrequency::Vertex;
            create_info.entry_point = "DebugDrawingDrawLinesVS";
            m_vs = GetRHI().CreateShader( create_info );
            VERIFY_NOT_EQUAL( m_vs, nullptr );

            create_info.frequency = RHI::ShaderFrequency::Pixel;
            create_info.entry_point = "DebugDrawingDrawLinesPS";
            m_ps = GetRHI().CreateShader( create_info );
            VERIFY_NOT_EQUAL( m_ps, nullptr );
        }
    }

    bool Run( RHICommandList& cmd_list, Rendergraph& rg, const Params& parms ) const
    {
        if ( !SE_ENSURE( parms.output && parms.view_descset && parms.indirect_args.buffer ) )
            return false;

        const RHIGraphicsPipeline* pso = GetPSO( parms.output->GetFormat() );

        if ( !SE_ENSURE( pso ) )
            return false;

        RHIPassRTVInfo rt = {};
        rt.load_op = RHILoadOp::PreserveContents;
        rt.store_op = RHIStoreOp::Store;
        rt.rtv = parms.output;

        RHIPassInfo pass_info = {};
        glm::uvec2 output_extent = glm::uvec2( parms.output->GetSize() );
        pass_info.render_area = RHIRect2D{ .offset = glm::ivec2( 0,0 ), .extent = output_extent };
        pass_info.render_targets = &rt;
        pass_info.render_targets_count = 1;
        cmd_list.BeginPass( pass_info );

        cmd_list.SetPSO( *pso );

        RHIViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = float( output_extent.x );
        viewport.height = float( output_extent.y );
        viewport.min_depth = 0.0f;
        viewport.max_depth = 1.0f;

        RHIRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = output_extent;

        cmd_list.SetViewports( 0, &viewport, 1 );
        cmd_list.SetScissors( 0, &scissor, 1 );

        RHIDescriptorSet* dummy_descset = rg.AllocateFrameDescSet( *m_dsl );
        cmd_list.BindDescriptorSet( 0, *dummy_descset );
        cmd_list.BindDescriptorSet( 1, *parms.view_descset );

        cmd_list.DrawIndirect( parms.indirect_args );

        cmd_list.EndPass();

        return true;
    }

private:

    const RHIGraphicsPipeline* GetPSO( RHIFormat output_format ) const
    {
        auto& pso = m_psos[output_format];

        if ( pso == nullptr )
        {
            RHIInputAssemblerInfo ia_info = {};
            ia_info.topology = RHIPrimitiveTopology::LineList;

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
            rhi_pipeline_info.rasterizer.line_width = 1.0f;

            pso = GetRHI().CreatePSO( rhi_pipeline_info );
        }

        return pso.get();
    }
};


// DebugDrawing

DebugDrawing::DebugDrawing( RHIDescriptorSetLayout* scene_view_dsl )
{
    m_program_init = std::make_unique<DebugDrawingInitProgram>();
    m_program_draw = std::make_unique<DebugDrawingDrawProgram>( scene_view_dsl );
}

DebugDrawing::~DebugDrawing() = default;

bool DebugDrawing::InitSceneViewData( DebugDrawingSceneViewData& data ) const
{
    {
        RHI::BufferInfo lines_buf_info = {};
        lines_buf_info.size = sizeof( GPUDebugLine ) * MAX_DEBUG_LINES;
        lines_buf_info.usage = RHIBufferUsageFlags::StructuredBuffer;
        data.m_lines_buf = GetRHI().CreateDeviceBuffer( lines_buf_info );
        if ( data.m_lines_buf == nullptr )
            return false;
    }

    {
        RHI::BufferInfo indirect_args_info = {};
        indirect_args_info.size = sizeof( GPUDebugLineIndirectArgs );
        indirect_args_info.usage = RHIBufferUsageFlags::StructuredBuffer | RHIBufferUsageFlags::IndirectArgs;
        data.m_lines_indirect_args_buf = GetRHI().CreateDeviceBuffer( indirect_args_info );
        if ( data.m_lines_indirect_args_buf == nullptr )
            return false;
    }

    return true;
}

void DebugDrawing::AddInitPasses( SceneViewFrameData& data, DebugDrawingContext& ctx ) const
{
    RGExternalBufferDesc buf_desc = {};
    buf_desc.name = "debug_lines_buf";
    buf_desc.rhi_buffer = data.view->GetDebugDrawData().m_lines_buf.get();
    ctx.lines_buf = data.rg->RegisterExternalBuffer( buf_desc );

    buf_desc.name = "debug_lines_indirect_args";
    buf_desc.rhi_buffer = data.view->GetDebugDrawData().m_lines_indirect_args_buf.get();
    ctx.lines_args_buf = data.rg->RegisterExternalBuffer( buf_desc );

    ctx.init_pass = data.rg->AddPass( RHI::QueueType::Graphics, "DebugDrawingInit" );

    // Init pass does not really use the buffer, but subsequent passes do. 
    // This allows to avoid having to call UseBuffer explicitly for every pass 
    ctx.init_pass->UseBuffer( *ctx.lines_buf, RGBufferUsage::ShaderWriteOnly );
    ctx.init_pass->UseBuffer( *ctx.lines_args_buf, RGBufferUsage::ShaderReadWrite );
}

void DebugDrawing::AddDrawPasses( SceneViewFrameData& data, DebugDrawingContext& ctx ) const
{
    // This pass is needed only to mark the end of debug draw buffers usage as accumulators.
    // Doing this helps to avoid having to call UseBuffer explicitly for every pass 
    ctx.finalize_collection_pass = data.rg->AddPass( RHI::QueueType::Graphics, "DebugDrawingFinalize" );
    ctx.finalize_collection_pass->UseBuffer( *ctx.lines_buf, RGBufferUsage::ShaderWriteOnly );
    ctx.finalize_collection_pass->UseBuffer( *ctx.lines_args_buf, RGBufferUsage::ShaderReadWrite );

    ctx.draw_pass = data.rg->AddPass( RHI::QueueType::Graphics, "DebugDrawingDrawLines" );
    ctx.draw_pass->UseBuffer( *ctx.lines_buf, RGBufferUsage::ShaderRead );
    ctx.draw_pass->UseBuffer( *ctx.lines_args_buf, RGBufferUsage::IndirectArgs );
    ctx.draw_pass->UseTextureView( *data.scene_output[data.scene_output_idx]->GetRTView() );
}

void DebugDrawing::RecordInitPasses( RHICommandList& cmd_list, const SceneViewFrameData& data, const DebugDrawingContext& ctx ) const
{
    ctx.init_pass->BorrowCommandList( cmd_list );

    DebugDrawingInitProgram::Params params = {};
    params.indirect_args_buf.buffer = ctx.lines_args_buf->GetRHIBuffer();

    m_program_init->Run( cmd_list, *data.rg, params );

    ctx.init_pass->EndPass();
}

void DebugDrawing::RecordDrawPasses( RHICommandList& cmd_list, const SceneViewFrameData& data, const DebugDrawingContext& ctx ) const
{
    ctx.draw_pass->BorrowCommandList( cmd_list );

    DebugDrawingDrawProgram::Params params = {};
    params.indirect_args.buffer = ctx.lines_args_buf->GetRHIBuffer();
    params.view_descset = data.view_desc_set;
    params.output = data.scene_output[data.scene_output_idx]->GetRTView()->GetRHIView();

    m_program_draw->Run( cmd_list, *data.rg, params );

    ctx.draw_pass->EndPass();
}
